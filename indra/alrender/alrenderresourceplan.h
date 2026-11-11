/**
 * @file alrenderresourceplan.h
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

#pragma once

#include "alrenderresourceinputs.h"
#include "alrendertargetdesc.h"

#include <array>
#include <bitset>
#include <optional>
#include <string_view>

/// What each of the pipeline's render resources is for a set of inputs: a description where the resource is wanted,
/// nothing where it is not, and for a target that takes its depth from another, which. A pure function of the inputs;
/// LLPipeline::reconcileResources makes persistent retained targets what the plan says, except for native shadow maps.
/// Native shadow generation consumes their descriptions directly. Native post, preview and bake transactions own
/// their short-lived resources independently of this plan.
///
/// Three packs of targets render a view each (LLPipeline::RenderTargetPack): Main, the world view at the render size;
/// Probe, a reflection probe's face at the probe resolution times its supersample; Hero, a mirror's face at the hero
/// resolution. Each pack is planned from its own inputs, so what changes one leaves the others alone. The main view
/// has targets of its own beside its pack, and the rest are fixed in size. The two probe managers' own resources are
/// planned beside their packs, from the same inputs.
class ALRenderResourcePlan
{
public:
    enum class Pack : U8
    {
        Main,
        Probe,
        Hero,
        COUNT
    };

    /// A pack's targets, the same in each. A pack plans only those it renders through.
    enum class Slot : U8
    {
        GBuffer,
        Screen,
        Sun0,
        Sun1,
        Sun2,
        Sun3,
        COUNT
    };

    static constexpr U32 SUN_CASCADES = 4;
    /// A capture's sun cascades, unless a probe's renders them all (ALRenderResourceInputs::mProbeAllCascades).
    static constexpr U32 CAPTURE_SUN_CASCADES = 2;

    // The fixed sizes, which the pipeline's passes read from here.
    static constexpr U32 PREVIEW_SIZE = 512;
    static constexpr U32 NOISE_SIZE = 128;

    /// Every resource, each pack's in Slot order first. The names are the debugger labels (name()).
    enum class Id : U8
    {
        MainGBuffer, MainScreen,
        MainSun0, MainSun1, MainSun2, MainSun3,

        ProbeGBuffer, ProbeScreen,
        ProbeSun0, ProbeSun1, ProbeSun2, ProbeSun3,

        HeroGBuffer, HeroScreen,
        HeroSun0, HeroSun1, HeroSun2, HeroSun3,

        // The main view's own, at its render size.
        SpotShadow0, SpotShadow1,

        // Standalone textures of the pipeline's own rather than render targets.
        // Noise and TrueNoise are native Metal publications. SMAA keeps these
        // identities only as plan vocabulary; its lookup textures are private
        // native-renderer resources.
        Noise,
        TrueNoise,
        SMAAArea,
        SMAASearch,
        SMAASample,

        COUNT
    };

    static constexpr size_t ID_COUNT = (size_t)Id::COUNT;
    using IdSet = std::bitset<ID_COUNT>;

    /// What a probe manager's own resources are built for: its cube array, its scratch target and its mip chain.
    struct ProbeManager
    {
        U32  mResolution  = 0; ///< a probe's face edge
        U32  mSuperSample = 1; ///< how much larger than that a face renders each way
        bool mHDR         = false;

        friend bool operator==(const ProbeManager&, const ProbeManager&) = default;
    };

    struct Resource
    {
        /// What the resource is, or nothing where it is not wanted and is freed. A standalone texture's
        /// (isTexture()) description names only its format: its size is its contents'.
        std::optional<ALRenderTargetDesc> mDesc;
        /// The resource whose depth this one's framebuffer shares; its own description then has no depth.
        std::optional<Id> mDepthFrom;

        friend bool operator==(const Resource&, const Resource&) = default;
    };

    static ALRenderResourcePlan make(const ALRenderResourceInputs& inputs);

    const Resource& operator[](Id id) const { return mResources[(size_t)id]; }
    bool wants(Id id) const { return mResources[(size_t)id].mDesc.has_value(); }
    const std::optional<ALRenderTargetDesc>& desc(Id id) const { return mResources[(size_t)id].mDesc; }
    /// Whether ordinary views trace the preceding native scene through reflection probes.
    /// This is logical render intent, independent of retained target allocations.
    bool usesScreenSpaceReflections() const { return mUsesScreenSpaceReflections; }

    /// The sun cascades @a pack renders, which are those it has maps for, from the first: SUN_CASCADES in the main view,
    /// CAPTURE_SUN_CASCADES in a capture unless a probe's renders them all, none without sun shadows.
    U32 sunCascades(Pack pack) const
    {
        U32 cascades = 0;
        while (cascades < SUN_CASCADES && wants(sunShadow(pack, cascades)))
        {
            ++cascades;
        }
        return cascades;
    }

    /// Whether the water drawn into @a pack refracts the scene beneath its surface. The native Metal water transaction
    /// owns its private scene-color and depth inputs. A probe's face clips the scene at the water and culls what lies
    /// beyond it (LLPipeline::isWaterClip), so its water is opaque: the fog of the water behind the surface, as
    /// everywhere with the setting off.
    static constexpr bool refractsWater(Pack pack, bool transparent_water)
    {
        return transparent_water && pack != Pack::Probe;
    }

    /// The reflection probes' manager's resources, or nothing where they are not wanted and are freed.
    const std::optional<ProbeManager>& reflectionProbes() const { return mReflectionProbes; }
    /// The mirrors' (LLHeroProbeManager's), likewise.
    const std::optional<ProbeManager>& heroProbes() const { return mHeroProbes; }

    /// The resources that differ between two plans: in their description, their depth's owner, or whether they are
    /// wanted at all.
    static IdSet changed(const ALRenderResourcePlan& before, const ALRenderResourcePlan& after);
    /// The resources whose contents, not their descriptions, are made from an input that differs between @a before
    /// and @a after, so that storage kept for them is filled again: the deferred noise map, from RenderDeferredNoise.
    static IdSet refilled(const ALRenderResourceInputs& before, const ALRenderResourceInputs& after);

    static constexpr Id inPack(Pack pack, Slot slot) { return (Id)((U32)pack * (U32)Slot::COUNT + (U32)slot); }
    static constexpr Id sunShadow(Pack pack, U32 cascade) { return inPack(pack, (Slot)((U32)Slot::Sun0 + cascade)); }

    /// The pack @a id is a target of, or nothing for the main view's own and the fixed ones.
    static std::optional<Pack> packOf(Id id);
    /// Its slot in its pack; only for a pack's targets.
    static Slot slotOf(Id id);
    /// What debuggers call it: main.gbuffer, probe.shadow.sun1, shadow.spot0, noise.
    static std::string_view name(Id id);
    /// Whether it is sized from the main view's render size, so that a resize, a snapshot and a retry at a lower size
    /// rebuild it, and its failing to build halves that size.
    static bool followsRenderSize(Id id);
    /// The retained main-view targets whose allocation failure drives a render-size retry, excluding shadow maps.
    static bool drivesRenderSize(Id id);
    /// Whether it is a shadow map: a pack's sun cascade or one of the spot lights' pair.
    static bool isShadowMap(Id id);
    /// Whether it is a standalone texture of the pipeline's own rather than a render target.
    static bool isTexture(Id id);

    /// Every id of @a pack.
    static IdSet idsOf(Pack pack);
    /// Every id for which @a predicate holds.
    template <typename Predicate> static IdSet idsWhere(Predicate predicate)
    {
        IdSet ids;
        for (size_t i = 0; i < ID_COUNT; ++i)
        {
            ids.set(i, predicate((Id)i));
        }
        return ids;
    }

private:
    Resource& at(Id id) { return mResources[(size_t)id]; }

    std::array<Resource, ID_COUNT> mResources{};
    bool mUsesScreenSpaceReflections = false;
    std::optional<ProbeManager>    mReflectionProbes;
    std::optional<ProbeManager>    mHeroProbes;
};

static_assert(ALRenderResourcePlan::inPack(ALRenderResourcePlan::Pack::Probe, ALRenderResourcePlan::Slot::GBuffer) ==
                  ALRenderResourcePlan::Id::ProbeGBuffer,
              "the probe pack's ids follow the main pack's, in Slot order");
static_assert(ALRenderResourcePlan::inPack(ALRenderResourcePlan::Pack::Hero, ALRenderResourcePlan::Slot::Sun3) ==
                  ALRenderResourcePlan::Id::HeroSun3,
              "the hero pack's ids follow the probe pack's, in Slot order");
static_assert((U32)ALRenderResourcePlan::Id::HeroSun3 + 1 ==
                  (U32)ALRenderResourcePlan::Id::SpotShadow0,
              "the main view's own ids follow the packs'");
