/**
 * @file alrenderresourceplan.cpp
 * @brief What each of the pipeline's render resources is for a set of inputs.
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

#include "alrenderresourceplan.h"

#include "llglheaders.h"

#include <algorithm>
#include <limits>

namespace
{
    using Plan  = ALRenderResourcePlan;
    using Id    = ALRenderResourcePlan::Id;
    using Pack  = ALRenderResourcePlan::Pack;
    using Slot  = ALRenderResourcePlan::Slot;
    using Depth = ALRenderTargetDesc::Depth;

    // A shadow map's edge: the size times its scale, plus 16 and rounded down to a multiple of 16, so it is even and
    // the horizontal shadow blur leaves no stripe.
    constexpr U32 blur_happy_size(U32 size, F32 scale)
    {
        return (U32)((F32)size * scale + 16.f) & ~0xFu;
    }

    // Each pack's slot names under its own prefix, laid out as Id lays the packs out.
    constexpr std::string_view PACK_NAMES[(size_t)Pack::COUNT][(size_t)Slot::COUNT] = {
        { "main.gbuffer", "main.screen", "main.shadow.sun0", "main.shadow.sun1", "main.shadow.sun2", "main.shadow.sun3" },
        { "probe.gbuffer", "probe.screen", "probe.shadow.sun0", "probe.shadow.sun1", "probe.shadow.sun2", "probe.shadow.sun3" },
        { "hero.gbuffer", "hero.screen", "hero.shadow.sun0", "hero.shadow.sun1", "hero.shadow.sun2", "hero.shadow.sun3" },
    };

    constexpr size_t PACK_IDS = (size_t)Pack::COUNT * (size_t)Slot::COUNT;

    // Everything after the packs, in Id order.
    constexpr std::string_view OWN_NAMES[Plan::ID_COUNT - PACK_IDS] = {
        "shadow.spot0", "shadow.spot1", "noise", "noise.true", "smaa.area", "smaa.search", "smaa.sample",
    };

    static_assert(OWN_NAMES[(size_t)Id::SMAASample - PACK_IDS] == "smaa.sample", "a name for every id after the packs");
}

// static
ALRenderResourcePlan ALRenderResourcePlan::make(const ALRenderResourceInputs& in)
{
    ALRenderResourcePlan plan;
    plan.mUsesScreenSpaceReflections = in.mSSR && in.mProbes;

    const bool hdr = in.mHDR;

    // The scene's depth carries native water exclusion/surface stencil bits and is float
    // under reverse-Z. The shadow maps are float under reverse-Z whatever AlchemyRenderShadowDepth32F says: reversed
    // depth puts the float mantissa's dense end where the perspective spot cascades need it, and DEPTH24 pads to the
    // same four bytes.
    const Depth main_depth   = in.mReverseZ ? Depth::D32F : Depth::D24;
    const Depth shadow_depth = (in.mShadowDepth32F || in.mReverseZ) ? Depth::D32F : Depth::D24;

    // Albedo, ORM or specular, the normal and the emissive. The normal's layout is the one the programs were compiled
    // against (GBUFFER_NORM_HDR), which setShaders latches from RenderHDREnabled before it compiles; nothing compiles
    // against the emissive's, which follows HDR as the other HDR targets do. R11F_G11F_B10F rather than RGB16F, which
    // pads to RGBA16F in memory: half the bandwidth, still float so it filters linearly and holds the sky writers'
    // boosts past 1, and unsigned is enough since the writers clamp emissive at zero. Its 6/6/5 mantissa bits band in
    // smooth gradients, which the sky, cloud and aurora writers' ditherEmissive covers.
    const std::array<U32, ALRenderTargetDesc::MAX_COLOR> gbuffer_color = {
        GL_SRGB8_ALPHA8, GL_RGBA8, in.mGBufferNormHDR ? (U32)GL_RGBA16 : (U32)GL_RGB10_A2,
        in.mEmissive ? (hdr ? (U32)GL_R11F_G11F_B10F : (U32)GL_RGB8) : 0u
    };

    struct PackShape
    {
        U32  width;
        U32  height;
        U32  shadow_width;
        U32  shadow_height;
        U32  sun_cascades;
    };

    const auto plan_pack = [&](Pack pack, const PackShape& shape)
    {
        const Id gbuffer = inPack(pack, Slot::GBuffer);
        plan.at(gbuffer).mDesc = ALRenderTargetDesc{ .width   = shape.width,
                                                     .height  = shape.height,
                                                     .color   = gbuffer_color,
                                                     .depth   = main_depth,
                                                     .stencil = true };
        // Always RGBA16F: its alpha carries glow through the post-deferred passes.
        plan.at(inPack(pack, Slot::Screen)) = {
            ALRenderTargetDesc{ .width = shape.width, .height = shape.height, .color = { GL_RGBA16F } }, gbuffer
        };
        if (in.mShadowDetail > 0)
        {
            for (U32 cascade = 0; cascade < shape.sun_cascades; ++cascade)
            {
                plan.at(sunShadow(pack, cascade)).mDesc = ALRenderTargetDesc{ .width  = shape.shadow_width,
                                                                              .height = shape.shadow_height,
                                                                              .depth  = shadow_depth };
            }
        }
    };

    // The main view, at the render size, with shadows scaled by RenderShadowResolutionScale.
    U32 width  = 0;
    U32 height = 0;
    in.renderSize(width, height);
    width  = std::max(width, 1u);
    height = std::max(height, 1u);
    const F32 shadow_scale = std::max(0.f, in.mShadowScale);
    plan_pack(Pack::Main, { width, height, blur_happy_size(width, shadow_scale), blur_happy_size(height, shadow_scale),
                            SUN_CASCADES });

    // A probe face renders at the probe resolution times its supersample, which the manager downsamples to its probe,
    // and its shadow maps at the size the full supersample of four gave them: a coarser capture is no reason for coarser
    // shadows in it. A capture renders two sun cascades, or a probe's all four where AlchemyRenderProbeAllShadowCascades
    // asks (generateSunShadow), and has maps for those alone. The native material-preview, water and light-map
    // transactions own their resources independently of this plan.
    if (in.mProbes)
    {
        const U32 probe_size     = in.mProbeResolution * in.mProbeSuperSample;
        const U32 probe_shadow   = blur_happy_size(in.mProbeResolution * 4, 1.f);
        const U32 probe_cascades = in.mProbeAllCascades ? SUN_CASCADES : CAPTURE_SUN_CASCADES;
        plan_pack(Pack::Probe,
                  { probe_size, probe_size, probe_shadow, probe_shadow, probe_cascades });
    }
    if (in.mProbes)
    {
        plan.mReflectionProbes = ProbeManager{ .mResolution = in.mProbeResolution,
                                               .mSuperSample = in.mProbeSuperSample,
                                               .mHDR = hdr };
    }

    // A mirror's face renders at the hero resolution, never supersampled, with two sun cascades, and runs no post chain.
    // The mirrors render only with reflection probes on (LLHeroProbeManager::update), so the pack and the manager's
    // resources are there only then.
    if (in.mMirrors && in.mProbes)
    {
        const U32 hero_size   = std::max(in.mHeroResolution, 1u);
        const U32 hero_shadow = blur_happy_size(hero_size, 1.f);
        plan_pack(Pack::Hero,
                  { hero_size, hero_size, hero_shadow, hero_shadow, CAPTURE_SUN_CASCADES });
        plan.mHeroProbes = ProbeManager{ .mResolution = hero_size, .mSuperSample = 1, .mHDR = hdr };
    }

    // The spot lights' shadows are the main view's alone, square at its width times the shadow scale, no larger than
    // their largest edge, and a texel at the least: a scale near 0 takes the shadows' detail, not the maps. A map covers
    // its light's frustum whatever the window's shape, so the edge bounds what a wide window or a snapshot costs them.
    if (in.mShadowDetail > 1)
    {
        // Bounded before it is made a U32, which a scale past that range would overflow.
        const U32 largest = in.mSpotShadowMaxResolution > 0 ? in.mSpotShadowMaxResolution : std::numeric_limits<U32>::max();
        const U32 spot    = std::max((U32)std::min((F64)width * shadow_scale, (F64)largest), 1u);
        for (Id id : { Id::SpotShadow0, Id::SpotShadow1 })
        {
            plan.at(id).mDesc = ALRenderTargetDesc{ .width = spot, .height = spot, .depth = shadow_depth };
        }
    }

    plan.at(Id::Noise).mDesc     = ALRenderTargetDesc{ .color = { GL_RGB16F } };
    plan.at(Id::TrueNoise).mDesc = ALRenderTargetDesc{ .color = { GL_RGB16F } };

    // SMAA's native transaction owns private area/search tables and publishes
    // SamplePath independently, so these legacy standalone identities remain
    // unplanned.

    return plan;
}

// static
ALRenderResourcePlan::IdSet ALRenderResourcePlan::changed(const ALRenderResourcePlan& before, const ALRenderResourcePlan& after)
{
    IdSet ids;
    for (size_t i = 0; i < ID_COUNT; ++i)
    {
        ids.set(i, before.mResources[i] != after.mResources[i]);
    }
    return ids;
}

// static
ALRenderResourcePlan::IdSet ALRenderResourcePlan::refilled(const ALRenderResourceInputs& before,
                                                           const ALRenderResourceInputs& after)
{
    IdSet ids;
    ids.set((size_t)Id::Noise, before.mDeferredNoise != after.mDeferredNoise);
    return ids;
}

// static
std::optional<ALRenderResourcePlan::Pack> ALRenderResourcePlan::packOf(Id id)
{
    if ((size_t)id >= PACK_IDS)
    {
        return std::nullopt;
    }
    return (Pack)((size_t)id / (size_t)Slot::COUNT);
}

// static
ALRenderResourcePlan::Slot ALRenderResourcePlan::slotOf(Id id)
{
    llassert((size_t)id < PACK_IDS);
    return (Slot)((size_t)id % (size_t)Slot::COUNT);
}

// static
std::string_view ALRenderResourcePlan::name(Id id)
{
    if ((size_t)id < PACK_IDS)
    {
        return PACK_NAMES[(size_t)id / (size_t)Slot::COUNT][(size_t)id % (size_t)Slot::COUNT];
    }
    if (id < Id::COUNT)
    {
        return OWN_NAMES[(size_t)id - PACK_IDS];
    }
    return {};
}

// static
bool ALRenderResourcePlan::followsRenderSize(Id id)
{
    if (const std::optional<Pack> pack = packOf(id))
    {
        return *pack == Pack::Main;
    }
    switch (id)
    {
        case Id::SpotShadow0:
        case Id::SpotShadow1:
            return true;
        default:
            return false;
    }
}

// static
bool ALRenderResourcePlan::drivesRenderSize(Id id)
{
    return followsRenderSize(id) && !isShadowMap(id);
}

// static
bool ALRenderResourcePlan::isShadowMap(Id id)
{
    if (packOf(id))
    {
        const Slot slot = slotOf(id);
        return slot >= Slot::Sun0 && slot <= Slot::Sun3;
    }
    return id == Id::SpotShadow0 || id == Id::SpotShadow1;
}

// static
bool ALRenderResourcePlan::isTexture(Id id)
{
    return id >= Id::Noise && id < Id::COUNT;
}

// static
bool ALRenderResourcePlan::describes(const ALRenderTargetDesc& desc) const
{
    return std::any_of(mResources.begin(), mResources.end(),
                       [&desc](const Resource& resource) { return resource.mDesc == desc && !resource.mDepthFrom; });
}

// static
ALRenderResourcePlan::IdSet ALRenderResourcePlan::idsOf(Pack pack)
{
    return idsWhere([pack](Id id) { return packOf(id) == pack; });
}
