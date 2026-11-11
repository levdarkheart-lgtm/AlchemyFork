/**
 * @file alrenderresourceplan_test.cpp
 * @brief What each render resource is for a set of inputs, and what each input changes.
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

#include "../alrenderresourceplan.h"

#include "llrendertarget.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using Plan   = ALRenderResourcePlan;
    using Inputs = ALRenderResourceInputs;
    using Id     = Plan::Id;
    using IdSet  = Plan::IdSet;
    using Pack   = Plan::Pack;
    using Slot   = Plan::Slot;
    using Depth  = ALRenderTargetDesc::Depth;

    // The reference configuration: HDR, reverse-Z, the emissive buffer, bloom at full scale with five mips, shadow
    // detail 2, SSAO, transparent water, reflection probes; SSR, FSAA and DoF off. Mirrors on, so the hero pack is there
    // to be changed or left alone.
    Inputs defaults()
    {
        Inputs in;
        in.mWidth   = 2560;
        in.mHeight  = 1440;
        in.mMirrors = true;
        return in;
    }

    // The ids in a set, by name, so a failed comparison says which resources differ.
    std::string names(const IdSet& ids)
    {
        std::string list;
        for (size_t i = 0; i < Plan::ID_COUNT; ++i)
        {
            if (ids.test(i))
            {
                if (!list.empty())
                {
                    list += ", ";
                }
                list += Plan::name((Id)i);
            }
        }
        return list.empty() ? std::string("nothing") : list;
    }

    IdSet of(std::initializer_list<Id> ids)
    {
        IdSet set;
        for (Id id : ids)
        {
            set.set((size_t)id);
        }
        return set;
    }

    IdSet in_packs(std::initializer_list<Pack> packs, std::initializer_list<Slot> slots)
    {
        IdSet set;
        for (Pack pack : packs)
        {
            for (Slot slot : slots)
            {
                set.set((size_t)Plan::inPack(pack, slot));
            }
        }
        return set;
    }

    IdSet sun_shadows(std::initializer_list<Pack> packs)
    {
        return in_packs(packs, { Slot::Sun0, Slot::Sun1, Slot::Sun2, Slot::Sun3 });
    }

    IdSet spot_shadows()
    {
        return of({ Id::SpotShadow0, Id::SpotShadow1 });
    }

    // Those of @a ids that @a plan wants.
    IdSet wanted(const Plan& plan, const IdSet& ids)
    {
        return Plan::idsWhere([&](Id id) { return ids.test((size_t)id) && plan.wants(id); });
    }

    IdSet render_sized()
    {
        return Plan::idsWhere(Plan::followsRenderSize);
    }

    // A shadow map's edge as the shadow blur needs it: the size times its scale plus 16, to a multiple of 16.
    U32 blur_happy(U32 size, F32 scale)
    {
        return (U32)((F32)size * scale + 16.f) & ~0xFu;
    }

    // One setting of plan section 1.2: the configuration it is changed from, the change, and exactly the resources
    // whose description that change must alter, given the plan before it.
    struct Row
    {
        const char*                       setting;
        std::function<void(Inputs&)>      from;
        std::function<void(Inputs&)>      change;
        std::function<IdSet(const Plan&)> expected;
    };

    void no_change(Inputs&) {}

    IdSet nothing(const Plan&)
    {
        return {};
    }

    std::vector<Row> setting_rows()
    {
        const auto hdr_off = [](Inputs& in)
        {
            in.mHDR           = false;
            in.mGBufferNormHDR = false;
        };

        return {
            { "RenderFSAAType to FXAA", no_change, [](Inputs& in) { in.mFSAAType = 1; }, nothing },
            { "RenderFSAAType to SMAA", no_change, [](Inputs& in) { in.mFSAAType = 2; },
              nothing },
            { "RenderFSAAType from SMAA to FXAA", [](Inputs& in) { in.mFSAAType = 2; },
              [](Inputs& in) { in.mFSAAType = 1; },
              nothing },
            { "RenderSMAAUseStencil under SMAA", [](Inputs& in) { in.mFSAAType = 2; },
              [](Inputs& in) { in.mSMAAStencil = false; },
              nothing },
            { "RenderSMAAUseStencil without SMAA", no_change, [](Inputs& in) { in.mSMAAStencil = false; }, nothing },
            { "RenderSMAAUseSample under SMAA", [](Inputs& in) { in.mFSAAType = 2; },
              [](Inputs& in) { in.mSMAAUseSample = true; },
              nothing },
            { "RenderSMAAUseSample without SMAA", no_change,
              [](Inputs& in) { in.mSMAAUseSample = true; }, nothing },
            { "RenderGlowResolutionPow under HDR", no_change, [](Inputs& in) { in.mGlowResolutionPow = 8; }, nothing },
            { "RenderGlowResolutionPow without HDR", hdr_off, [](Inputs& in) { in.mGlowResolutionPow = 8; },
              nothing },
            { "RenderGlowHDR under HDR", no_change, [](Inputs& in) { in.mGlowHDR = true; }, nothing },
            { "RenderGlowHDR without HDR", hdr_off, [](Inputs& in) { in.mGlowHDR = true; },
              nothing },
            { "RenderBloomMipCount", no_change, [](Inputs& in) { in.mBloomMips = 4; },
              nothing },
            { "RenderBloomResolutionScale", no_change, [](Inputs& in) { in.mBloomScale = 2; },
              nothing },
            { "RenderBloomHalation", no_change, [](Inputs& in) { in.mBloomHalation = true; },
              nothing },
            { "RenderEnableEmissiveBuffer", no_change, [](Inputs& in) { in.mEmissive = false; },
              [](const Plan&) { return of({ Id::MainGBuffer, Id::ProbeGBuffer, Id::HeroGBuffer }); } },
            { "RenderHDREnabled", no_change, hdr_off,
              [](const Plan&) { return in_packs({ Pack::Main, Pack::Probe, Pack::Hero }, { Slot::GBuffer }); } },
            { "AlchemyRenderReverseZ", no_change, [](Inputs& in) { in.mReverseZ = false; },
              [](const Plan& before)
              {
                  return of({ Id::MainGBuffer, Id::ProbeGBuffer, Id::HeroGBuffer, Id::SpotShadow0, Id::SpotShadow1 }) |
                         wanted(before, sun_shadows({ Pack::Main, Pack::Probe, Pack::Hero }));
              } },
            { "AlchemyRenderReverseZ over 32-bit shadows", [](Inputs& in) { in.mShadowDepth32F = true; },
              [](Inputs& in) { in.mReverseZ = false; },
              [](const Plan&)
              {
                  return of({ Id::MainGBuffer, Id::ProbeGBuffer, Id::HeroGBuffer });
              } },
            { "RenderShadowDetail 2 to 1", no_change, [](Inputs& in) { in.mShadowDetail = 1; },
              [](const Plan&) { return of({ Id::SpotShadow0, Id::SpotShadow1 }); } },
            { "RenderShadowDetail 2 to 0", no_change, [](Inputs& in) { in.mShadowDetail = 0; },
              [](const Plan& before)
              {
                  return wanted(before, sun_shadows({ Pack::Main, Pack::Probe, Pack::Hero })) |
                         of({ Id::SpotShadow0, Id::SpotShadow1 });
              } },
            { "RenderDeferredSSAO with shadows", no_change, [](Inputs& in) { in.mSSAO = false; },
              nothing },
            { "RenderDeferredSSAO without shadows", [](Inputs& in) { in.mShadowDetail = 0; },
              [](Inputs& in) { in.mSSAO = false; }, nothing },
            { "RenderDeferredSSAO without compute", [](Inputs& in) { in.mCompute = false; },
              [](Inputs& in) { in.mSSAO = false; }, nothing },
            { "AlchemyRenderDisableCompute", no_change, [](Inputs& in) { in.mCompute = false; },
              nothing },
            { "AlchemyRenderDisableCompute without SSAO", [](Inputs& in) { in.mSSAO = false; },
              [](Inputs& in) { in.mCompute = false; }, nothing },
            { "RenderShadowResolutionScale", no_change, [](Inputs& in) { in.mShadowScale = 0.5f; },
              [](const Plan&) { return sun_shadows({ Pack::Main }) | of({ Id::SpotShadow0, Id::SpotShadow1 }); } },
            { "AlchemyRenderShadowDepth32F under reverse-Z", no_change, [](Inputs& in) { in.mShadowDepth32F = true; },
              nothing },
            { "AlchemyRenderShadowDepth32F under forward-Z", [](Inputs& in) { in.mReverseZ = false; },
              [](Inputs& in) { in.mShadowDepth32F = true; },
              [](const Plan& before)
              {
                  return wanted(before, sun_shadows({ Pack::Main, Pack::Probe, Pack::Hero })) |
                         of({ Id::SpotShadow0, Id::SpotShadow1 });
              } },
            { "AlchemyRenderProbeAllShadowCascades", no_change, [](Inputs& in) { in.mProbeAllCascades = true; },
              [](const Plan&) { return of({ Id::ProbeSun2, Id::ProbeSun3 }); } },
            { "AlchemyRenderProbeAllShadowCascades without shadows", [](Inputs& in) { in.mShadowDetail = 0; },
              [](Inputs& in) { in.mProbeAllCascades = true; }, nothing },
            { "AlchemyRenderProbeAllShadowCascades without probes", [](Inputs& in) { in.mProbes = false; },
              [](Inputs& in) { in.mProbeAllCascades = true; }, nothing },
            { "ALProbeSuperSample", no_change, [](Inputs& in) { in.mProbeSuperSample = 4; },
              [](const Plan& before)
              {
                  return wanted(before, Plan::idsOf(Pack::Probe) & ~sun_shadows({ Pack::Probe }));
              } },
            { "RenderReflectionProbeResolution", no_change, [](Inputs& in) { in.mProbeResolution = 256; },
              [](const Plan& before) { return wanted(before, Plan::idsOf(Pack::Probe)); } },
            { "RenderReflectionsEnabled", no_change, [](Inputs& in) { in.mProbes = false; },
              [](const Plan& before) { return wanted(before, Plan::idsOf(Pack::Probe) | Plan::idsOf(Pack::Hero)); } },
            { "RenderScreenSpaceReflections", no_change, [](Inputs& in) { in.mSSR = true; },
              nothing },
            { "RenderScreenSpaceReflections without probes", [](Inputs& in) { in.mProbes = false; },
              [](Inputs& in) { in.mSSR = true; }, nothing },
            { "RenderMirrors", no_change, [](Inputs& in) { in.mMirrors = false; },
              [](const Plan& before) { return wanted(before, Plan::idsOf(Pack::Hero)); } },
            { "RenderHeroProbeResolution", no_change, [](Inputs& in) { in.mHeroResolution = 512; },
              [](const Plan& before) { return wanted(before, Plan::idsOf(Pack::Hero)); } },
            { "the window's size", no_change,
              [](Inputs& in)
              {
                  in.mWidth  = 1920;
                  in.mHeight = 1080;
              },
              [](const Plan& before) { return wanted(before, render_sized()); } },
            { "RenderResolutionDivisor", no_change, [](Inputs& in) { in.mResolutionDivisor = 2; },
              [](const Plan& before) { return wanted(before, render_sized()); } },
            { "RenderResolutionMultiplier", no_change, [](Inputs& in) { in.mResolutionMultiplier = 1.5f; },
              [](const Plan& before) { return wanted(before, render_sized()) & ~spot_shadows(); } },
            { "RenderResolutionMultiplier below the spot shadows' edge", no_change,
              [](Inputs& in) { in.mResolutionMultiplier = 0.75f; },
              [](const Plan& before) { return wanted(before, render_sized()); } },
            { "AlchemyRenderSpotShadowMaxResolution", no_change, [](Inputs& in) { in.mSpotShadowMaxResolution = 1024; },
              [](const Plan&) { return spot_shadows(); } },
            { "AlchemyRenderSpotShadowMaxResolution to none", no_change, [](Inputs& in) { in.mSpotShadowMaxResolution = 0; },
              [](const Plan&) { return spot_shadows(); } },
            { "AlchemyRenderSpotShadowMaxResolution past the render width", [](Inputs& in) { in.mSpotShadowMaxResolution = 4096; },
              [](Inputs& in) { in.mSpotShadowMaxResolution = 8192; }, nothing },
            { "AlchemyRenderSpotShadowMaxResolution without spot shadows", [](Inputs& in) { in.mShadowDetail = 1; },
              [](Inputs& in) { in.mSpotShadowMaxResolution = 1024; }, nothing },
        };
    }

    // Changes to main-view geometry and native post policy leave the retained capture packs alone.
    std::vector<std::pair<const char*, std::function<void(Inputs&)>>> not_the_probes()
    {
        return {
            { "the window's size", [](Inputs& in) { in.mWidth = 3840; in.mHeight = 2160; } },
            { "RenderResolutionDivisor", [](Inputs& in) { in.mResolutionDivisor = 2; } },
            { "RenderResolutionMultiplier", [](Inputs& in) { in.mResolutionMultiplier = 0.5f; } },
            { "a snapshot's size", [](Inputs& in) { in.mWidth = 7680; in.mHeight = 4320; } },
            { "FXAA", [](Inputs& in) { in.mFSAAType = 1; } },
            { "SMAA", [](Inputs& in) { in.mFSAAType = 2; } },
            { "SMAA without its stencil", [](Inputs& in) { in.mFSAAType = 2; in.mSMAAStencil = false; } },
            { "RenderBloomMipCount", [](Inputs& in) { in.mBloomMips = 7; } },
            { "RenderBloomResolutionScale", [](Inputs& in) { in.mBloomScale = 3; } },
            { "RenderBloomHalation", [](Inputs& in) { in.mBloomHalation = true; } },
            { "RenderGlowResolutionPow", [](Inputs& in) { in.mGlowResolutionPow = 7; } },
            { "RenderGlowHDR", [](Inputs& in) { in.mGlowHDR = true; } },
            { "RenderScreenSpaceReflections", [](Inputs& in) { in.mSSR = true; } },
            { "RenderDeferredSSAO with shadows", [](Inputs& in) { in.mSSAO = false; } },
            { "RenderShadowResolutionScale", [](Inputs& in) { in.mShadowScale = 2.f; } },
        };
    }

    TEST_CASE("alrenderresourceplan: each setting changes exactly the resources it describes", "[alrenderresourceplan]")
    {
        for (const Row& row : setting_rows())
        {
            INFO(row.setting);
            Inputs from = defaults();
            row.from(from);
            Inputs to = from;
            row.change(to);

            const Plan before = Plan::make(from);
            const Plan after  = Plan::make(to);
            CHECK(names(Plan::changed(before, after)) == names(row.expected(before)));
        }
    }

    TEST_CASE("alrenderresourceplan: the noise strength fills the noise map again and describes nothing",
              "[alrenderresourceplan]")
    {
        const Inputs from = defaults();
        Inputs       to   = from;
        to.mDeferredNoise = from.mDeferredNoise * 2.f;

        CHECK(names(Plan::changed(Plan::make(from), Plan::make(to))) == "nothing");
        CHECK(names(Plan::refilled(from, to)) == names(of({ Id::Noise })));
        CHECK(names(Plan::refilled(from, from)) == "nothing");
    }

    TEST_CASE("alrenderresourceplan: no other setting fills anything again", "[alrenderresourceplan]")
    {
        for (const Row& row : setting_rows())
        {
            INFO(row.setting);
            Inputs from = defaults();
            row.from(from);
            Inputs to = from;
            row.change(to);
            CHECK(names(Plan::refilled(from, to)) == "nothing");
        }
    }

    TEST_CASE("alrenderresourceplan: a resize leaves the probe packs and the fixed-size targets alone", "[alrenderresourceplan]")
    {
        const std::vector<std::pair<const char*, std::function<void(Inputs&)>>> configurations = {
            { "the defaults", no_change },
            { "SMAA and SSR", [](Inputs& in) { in.mFSAAType = 2; in.mSSR = true; } },
            { "no HDR", [](Inputs& in) { in.mHDR = false; in.mGBufferNormHDR = false; } },
            { "no shadows", [](Inputs& in) { in.mShadowDetail = 0; } },
        };
        const std::vector<std::pair<const char*, std::function<void(Inputs&)>>> resizes = {
            { "to 1920x1080", [](Inputs& in) { in.mWidth = 1920; in.mHeight = 1080; } },
            { "to 3840x2160", [](Inputs& in) { in.mWidth = 3840; in.mHeight = 2160; } },
            { "to a snapshot's 6000x4000", [](Inputs& in) { in.mWidth = 6000; in.mHeight = 4000; } },
            { "by a divisor of 2", [](Inputs& in) { in.mResolutionDivisor = 2; } },
            { "by a divisor of 3", [](Inputs& in) { in.mResolutionDivisor = 3; } },
            { "by a multiplier of 0.5", [](Inputs& in) { in.mResolutionMultiplier = 0.5f; } },
            { "by a multiplier of 2", [](Inputs& in) { in.mResolutionMultiplier = 2.f; } },
        };

        for (const auto& [configuration, configure] : configurations)
        {
            for (const auto& [resize, apply] : resizes)
            {
                INFO(configuration << ", resized " << resize);
                Inputs from = defaults();
                configure(from);
                Inputs to = from;
                apply(to);

                const IdSet changed = Plan::changed(Plan::make(from), Plan::make(to));
                CHECK(changed.any());
                CHECK(names(changed & ~render_sized()) == "nothing");
            }
        }
    }

    TEST_CASE("alrenderresourceplan: shadow maps are sized from the size the pack renders at", "[alrenderresourceplan]")
    {
        struct Case
        {
            U32 width;
            U32 height;
            U32 divisor;
            F32 multiplier;
            F32 scale;
        };
        const Case cases[] = {
            { 2560, 1440, 1, 1.f, 1.f },  { 2560, 1440, 2, 1.f, 1.f },   { 2560, 1440, 3, 1.f, 0.75f },
            { 1920, 1080, 1, 1.5f, 1.f }, { 1920, 1080, 1, 0.5f, 2.f },  { 3840, 2160, 4, 1.f, 1.5f },
            { 1366, 768, 1, 2.f, 0.5f },  { 800, 600, 1, 1.f, 0.25f },
        };

        for (const Case& c : cases)
        {
            INFO(c.width << "x" << c.height << ", divisor " << c.divisor << ", multiplier " << c.multiplier << ", scale "
                         << c.scale);
            Inputs in               = defaults();
            in.mWidth               = c.width;
            in.mHeight              = c.height;
            in.mResolutionDivisor   = c.divisor;
            in.mResolutionMultiplier = c.multiplier;
            in.mShadowScale         = c.scale;
            const Plan plan         = Plan::make(in);

            REQUIRE(plan.desc(Id::MainGBuffer));
            const ALRenderTargetDesc& gbuffer = *plan.desc(Id::MainGBuffer);
            U32 width  = c.width;
            U32 height = c.height;
            Inputs::scaleToRenderResolution(width, height, c.divisor, c.multiplier);
            CHECK(gbuffer.width == width);
            CHECK(gbuffer.height == height);

            for (U32 cascade = 0; cascade < Plan::SUN_CASCADES; ++cascade)
            {
                REQUIRE(plan.desc(Plan::sunShadow(Pack::Main, cascade)));
                const ALRenderTargetDesc& sun = *plan.desc(Plan::sunShadow(Pack::Main, cascade));
                CHECK(sun.width == blur_happy(gbuffer.width, c.scale));
                CHECK(sun.height == blur_happy(gbuffer.height, c.scale));
            }
            for (Id spot : { Id::SpotShadow0, Id::SpotShadow1 })
            {
                REQUIRE(plan.desc(spot));
                CHECK(plan.desc(spot)->width == std::min((U32)((F32)gbuffer.width * c.scale), in.mSpotShadowMaxResolution));
                CHECK(plan.desc(spot)->height == plan.desc(spot)->width);
            }

            // The capture packs' shadows never take the scale, the divisor or the multiplier.
            REQUIRE(plan.desc(Id::ProbeSun0));
            CHECK(plan.desc(Id::ProbeSun0)->width == blur_happy(in.mProbeResolution * 4, 1.f));
            REQUIRE(plan.desc(Id::HeroSun0));
            CHECK(plan.desc(Id::HeroSun0)->width == blur_happy(in.mHeroResolution, 1.f));
        }
    }

    TEST_CASE("alrenderresourceplan: the spot shadows stop at their largest edge", "[alrenderresourceplan]")
    {
        struct Case
        {
            U32 width;
            U32 largest;
            F32 scale;
            U32 expected;
        };
        const Case cases[] = {
            { 3440, 2048, 1.f, 2048 }, { 1920, 2048, 1.f, 1920 }, { 3440, 4096, 1.f, 3440 }, { 3440, 0, 1.f, 3440 },
            { 3440, 2048, 0.5f, 1720 }, { 1920, 2048, 1.5f, 2048 }, { 3440, 1, 1.f, 1 },     { 2048, 2048, 1.f, 2048 },
        };

        for (const Case& c : cases)
        {
            INFO(c.width << " wide, largest " << c.largest << ", scale " << c.scale);
            Inputs in                   = defaults();
            in.mWidth                   = c.width;
            in.mHeight                  = 1440;
            in.mSpotShadowMaxResolution = c.largest;
            in.mShadowScale             = c.scale;
            const Plan plan             = Plan::make(in);
            for (Id spot : { Id::SpotShadow0, Id::SpotShadow1 })
            {
                REQUIRE(plan.desc(spot));
                CHECK(plan.desc(spot)->width == c.expected);
                CHECK(plan.desc(spot)->height == c.expected);
            }
            // The sun's cascades follow the render size as they did.
            REQUIRE(plan.desc(Id::MainSun0));
            CHECK(plan.desc(Id::MainSun0)->width == blur_happy(c.width, c.scale));
        }

        // A snapshot past the edge leaves the spot pair as the window had it.
        Inputs window   = defaults();
        window.mWidth   = 3440;
        Inputs snapshot = window;
        snapshot.mWidth  = 6000;
        snapshot.mHeight = 4000;
        CHECK(names(Plan::changed(Plan::make(window), Plan::make(snapshot)) & spot_shadows()) == "nothing");
    }

    TEST_CASE("alrenderresourceplan: every target is planned with a size, whatever the shadow scale", "[alrenderresourceplan]")
    {
        // A target with no size cannot be built, and a target the render size sizes failing to build halves that size.
        for (F32 scale : { 0.f, 0.0001f, 0.01f, 0.25f, 1.f, 4.f })
        {
            for (const auto& [width, height] : { std::pair{ 2560u, 1440u }, std::pair{ 64u, 48u }, std::pair{ 1u, 1u } })
            {
                INFO(width << "x" << height << ", shadow scale " << scale);
                Inputs in       = defaults();
                in.mWidth       = width;
                in.mHeight      = height;
                in.mShadowScale = scale;
                const Plan plan = Plan::make(in);
                for (size_t i = 0; i < Plan::ID_COUNT; ++i)
                {
                    const Id id = (Id)i;
                    if (Plan::isTexture(id) || !plan.wants(id))
                    {
                        continue;
                    }
                    INFO(Plan::name(id));
                    CHECK(plan.desc(id)->width > 0);
                    CHECK(plan.desc(id)->height > 0);
                }
                CHECK(plan.wants(Id::SpotShadow0));
            }
        }
    }

    TEST_CASE("alrenderresourceplan: every pack's shadows take one depth format", "[alrenderresourceplan]")
    {
        for (bool reverse_z : { false, true })
        {
            for (bool depth_32f : { false, true })
            {
                INFO("reverse-Z " << reverse_z << ", 32-bit shadows " << depth_32f);
                Inputs in          = defaults();
                in.mReverseZ       = reverse_z;
                in.mShadowDepth32F = depth_32f;
                const Plan plan    = Plan::make(in);

                const Depth expected = reverse_z || depth_32f ? Depth::D32F : Depth::D24;
                for (Pack pack : { Pack::Main, Pack::Probe, Pack::Hero })
                {
                    REQUIRE(plan.sunCascades(pack) > 0);
                    for (U32 cascade = 0; cascade < plan.sunCascades(pack); ++cascade)
                    {
                        REQUIRE(plan.desc(Plan::sunShadow(pack, cascade)));
                        CHECK(plan.desc(Plan::sunShadow(pack, cascade))->depth == expected);
                    }
                }
                REQUIRE(plan.desc(Id::SpotShadow0));
                CHECK(plan.desc(Id::SpotShadow0)->depth == expected);
            }
        }
    }

    TEST_CASE("alrenderresourceplan: each pack has maps for the sun cascades it renders", "[alrenderresourceplan]")
    {
        struct Case
        {
            const char*                  configuration;
            std::function<void(Inputs&)> configure;
            U32                          main;
            U32                          probe;
            U32                          hero;
        };
        // Two in a capture, as sampleDirectionalShadow reads either all four or the first two.
        const Case cases[] = {
            { "the defaults", no_change, 4, 2, 2 },
            { "all of a probe's cascades", [](Inputs& in) { in.mProbeAllCascades = true; }, 4, 4, 2 },
            { "the sun's shadows alone", [](Inputs& in) { in.mShadowDetail = 1; }, 4, 2, 2 },
            { "no shadows", [](Inputs& in) { in.mShadowDetail = 0; }, 0, 0, 0 },
            { "no shadows, all of a probe's cascades",
              [](Inputs& in)
              {
                  in.mShadowDetail     = 0;
                  in.mProbeAllCascades = true;
              },
              0, 0, 0 },
            { "no probes", [](Inputs& in) { in.mProbes = false; }, 4, 0, 0 },
        };

        for (const Case& c : cases)
        {
            Inputs in = defaults();
            c.configure(in);
            const Plan plan = Plan::make(in);

            for (const auto& [pack, cascades] :
                 { std::pair{ Pack::Main, c.main }, std::pair{ Pack::Probe, c.probe }, std::pair{ Pack::Hero, c.hero } })
            {
                INFO(c.configuration << ", pack " << (U32)pack);
                CHECK(plan.sunCascades(pack) == cascades);
                // From the first, and none past them.
                for (U32 cascade = 0; cascade < Plan::SUN_CASCADES; ++cascade)
                {
                    INFO("cascade " << cascade);
                    CHECK(plan.wants(Plan::sunShadow(pack, cascade)) == (cascade < cascades));
                }
            }
        }
    }

    TEST_CASE("alrenderresourceplan: the probe and hero packs ignore what is the main view's", "[alrenderresourceplan]")
    {
        for (const auto& [change, apply] : not_the_probes())
        {
            for (bool hdr : { true, false })
            {
                INFO(change << (hdr ? " under HDR" : " without HDR"));
                Inputs from          = defaults();
                from.mHDR            = hdr;
                from.mGBufferNormHDR = hdr;
                Inputs to            = from;
                apply(to);

                const Plan before   = Plan::make(from);
                const Plan after    = Plan::make(to);
                const IdSet changed = Plan::changed(before, after);
                CHECK(names(changed & Plan::idsOf(Pack::Hero)) == "nothing");
                CHECK(names(changed & Plan::idsOf(Pack::Probe)) == "nothing");
                CHECK(before.reflectionProbes() == after.reflectionProbes());
                CHECK(before.heroProbes() == after.heroProbes());
            }
        }
    }

    TEST_CASE("alrenderresourceplan: HDR emissive and reverse-Z rebuild all three G-buffers", "[alrenderresourceplan]")
    {
        const std::vector<std::pair<const char*, std::function<void(Inputs&)>>> changes = {
            { "RenderHDREnabled", [](Inputs& in) { in.mHDR = false; in.mGBufferNormHDR = false; } },
            { "RenderEnableEmissiveBuffer", [](Inputs& in) { in.mEmissive = false; } },
            { "AlchemyRenderReverseZ", [](Inputs& in) { in.mReverseZ = false; } },
        };
        for (const auto& [change, apply] : changes)
        {
            INFO(change);
            Inputs to = defaults();
            apply(to);
            const IdSet changed = Plan::changed(Plan::make(defaults()), Plan::make(to));
            CHECK(changed.test((size_t)Id::MainGBuffer));
            CHECK(changed.test((size_t)Id::ProbeGBuffer));
            CHECK(changed.test((size_t)Id::HeroGBuffer));
        }
    }

    TEST_CASE("alrenderresourceplan: the probe resolution and supersample change the probe pack alone", "[alrenderresourceplan]")
    {
        const std::vector<std::pair<const char*, std::function<void(Inputs&)>>> changes = {
            { "RenderReflectionProbeResolution up", [](Inputs& in) { in.mProbeResolution = 512; } },
            { "RenderReflectionProbeResolution down", [](Inputs& in) { in.mProbeResolution = 64; } },
            { "ALProbeSuperSample up", [](Inputs& in) { in.mProbeSuperSample = 4; } },
            { "ALProbeSuperSample down", [](Inputs& in) { in.mProbeSuperSample = 1; } },
        };
        for (const auto& [change, apply] : changes)
        {
            INFO(change);
            Inputs to = defaults();
            apply(to);
            const Plan before   = Plan::make(defaults());
            const Plan after    = Plan::make(to);
            const IdSet changed = Plan::changed(before, after);
            CHECK(changed.test((size_t)Id::ProbeGBuffer));
            CHECK(names(changed & ~Plan::idsOf(Pack::Probe)) == "nothing");
            CHECK(before.reflectionProbes() != after.reflectionProbes());
            CHECK(before.heroProbes() == after.heroProbes());
        }
    }

    TEST_CASE("alrenderresourceplan: the hero resolution changes the hero pack alone", "[alrenderresourceplan]")
    {
        for (U32 resolution : { 256u, 512u, 2048u })
        {
            INFO("RenderHeroProbeResolution " << resolution);
            Inputs to          = defaults();
            to.mHeroResolution = resolution;
            const Plan before   = Plan::make(defaults());
            const Plan after    = Plan::make(to);
            const IdSet changed = Plan::changed(before, after);
            CHECK(changed.test((size_t)Id::HeroGBuffer));
            CHECK(names(changed & ~Plan::idsOf(Pack::Hero)) == "nothing");
            CHECK(before.heroProbes() != after.heroProbes());
            CHECK(before.reflectionProbes() == after.reflectionProbes());
        }
    }

    TEST_CASE("alrenderresourceplan: with probes off there is no probe pack and no probe manager resources", "[alrenderresourceplan]")
    {
        for (bool mirrors : { false, true })
        {
            INFO("mirrors " << mirrors);
            Inputs in       = defaults();
            in.mProbes      = false;
            in.mMirrors     = mirrors;
            const Plan plan = Plan::make(in);

            CHECK(names(wanted(plan, Plan::idsOf(Pack::Probe) | Plan::idsOf(Pack::Hero))) == "nothing");
            CHECK(!plan.reflectionProbes());
            CHECK(!plan.heroProbes());
        }
    }

    // An independent retained allocation matches; a color target borrowing depth does not.
    TEST_CASE("alrenderresourceplan: describes() knows the descriptions of the resources it wants", "[alrenderresourceplan]")
    {
        const Plan plan = Plan::make(defaults());
        const ALRenderTargetDesc& gbuffer = *plan.desc(Id::MainGBuffer);
        CHECK(plan.describes(gbuffer));
        ALRenderTargetDesc smaller = gbuffer;
        smaller.width /= 2;
        CHECK_FALSE(plan.describes(smaller));
        UNSCOPED_INFO("the screen shares the G-buffer's depth, so its colour alone is no resource's description");
        REQUIRE(plan[Id::MainScreen].mDepthFrom);
        CHECK_FALSE(plan.describes(*plan.desc(Id::MainScreen)));
    }

    TEST_CASE("alrenderresourceplan: the hero pack and its probes are there only with mirrors and probes both on", "[alrenderresourceplan]")
    {
        for (bool mirrors : { false, true })
        {
            for (bool probes : { false, true })
            {
                INFO("mirrors " << mirrors << ", probes " << probes);
                Inputs in       = defaults();
                in.mMirrors     = mirrors;
                in.mProbes      = probes;
                const Plan plan = Plan::make(in);
                CHECK(plan.wants(Id::HeroGBuffer) == (mirrors && probes));
                CHECK(plan.heroProbes().has_value() == (mirrors && probes));
                CHECK(wanted(plan, Plan::idsOf(Pack::Hero)).any() == (mirrors && probes));
            }
        }
    }

    TEST_CASE("alrenderresourceplan: each probe manager is planned from its own inputs", "[alrenderresourceplan]")
    {
        for (bool hdr : { true, false })
        {
            INFO((hdr ? "under HDR" : "without HDR"));
            Inputs in            = defaults();
            in.mHDR              = hdr;
            in.mGBufferNormHDR   = hdr;
            in.mProbeResolution  = 256;
            in.mProbeSuperSample = 4;
            in.mHeroResolution   = 512;
            const Plan plan      = Plan::make(in);

            REQUIRE(plan.reflectionProbes());
            CHECK(plan.reflectionProbes()->mResolution == 256);
            CHECK(plan.reflectionProbes()->mSuperSample == 4);
            CHECK(plan.reflectionProbes()->mHDR == hdr);
            REQUIRE(plan.heroProbes());
            CHECK(plan.heroProbes()->mResolution == 512);
            CHECK(plan.heroProbes()->mSuperSample == 1);
            CHECK(plan.heroProbes()->mHDR == hdr);

            // A probe face is the probe pack's size, and a mirror's face the hero pack's.
            REQUIRE(plan.desc(Id::ProbeGBuffer));
            CHECK(plan.desc(Id::ProbeGBuffer)->width == 256 * 4);
            REQUIRE(plan.desc(Id::HeroGBuffer));
            CHECK(plan.desc(Id::HeroGBuffer)->width == 512);
        }
    }

    // A probe's face clips the scene at the water (LLPipeline::isWaterClip), so its water draws opaque; the main view
    // and a mirror's face refract where the setting asks. The transaction's private Metal resources are outside this
    // retained-resource plan.
    TEST_CASE("alrenderresourceplan: native water refraction policy distinguishes capture packs", "[alrenderresourceplan]")
    {
        for (bool transparent : { true, false })
        {
            INFO("RenderTransparentWater " << transparent);
            CHECK(Plan::refractsWater(Pack::Main, transparent) == transparent);
            CHECK(Plan::refractsWater(Pack::Hero, transparent) == transparent);
            CHECK_FALSE(Plan::refractsWater(Pack::Probe, transparent));
        }
    }

    TEST_CASE("alrenderresourceplan: SSAO is private to the Metal transaction and changes no retained resource",
              "[alrenderresourceplan]")
    {
        for (S32 shadow_detail : { 0, 1, 2 })
        {
            INFO("shadow detail " << shadow_detail);
            Inputs from        = defaults();
            from.mShadowDetail = shadow_detail;
            Inputs to          = from;
            to.mSSAO           = !from.mSSAO;
            const IdSet changed = Plan::changed(Plan::make(from), Plan::make(to));
            CHECK(names(changed) == "nothing");
        }
    }

    TEST_CASE("alrenderresourceplan: native SMAA owns no standalone lookup resources",
              "[alrenderresourceplan]")
    {
        for (U32 fsaa : { 0u, 1u, 2u })
        {
            for (bool stencil : { false, true })
            {
                for (bool sample : { false, true })
                {
                    INFO("RenderFSAAType " << fsaa << ", stencil " << stencil
                         << ", sample " << sample);
                    Inputs in         = defaults();
                    in.mFSAAType      = fsaa;
                    in.mSMAAStencil   = stencil;
                    in.mSMAAUseSample = sample;
                    const Plan plan   = Plan::make(in);

                    for (Id table : { Id::SMAAArea, Id::SMAASearch, Id::SMAASample })
                    {
                        INFO(Plan::name(table));
                        CHECK_FALSE(plan.wants(table));
                    }
                }
            }
        }
    }

    TEST_CASE("alrenderresourceplan: SSR intent is independent of retained target descriptions",
              "[alrenderresourceplan]")
    {
        for (bool probes : { false, true })
        {
            Inputs before = defaults(); before.mProbes = probes; before.mSSR = false;
            Inputs after = before; after.mSSR = true;
            const Plan off = Plan::make(before), on = Plan::make(after);
            CHECK_FALSE(off.usesScreenSpaceReflections());
            CHECK(on.usesScreenSpaceReflections() == probes);
            CHECK(Plan::changed(off, on).none());
            CHECK(Plan::changed(on, off).none());
        }
    }

    TEST_CASE("alrenderresourceplan: a target shares depth only with an owner of its own size", "[alrenderresourceplan]")
    {
        const std::vector<std::function<void(Inputs&)>> configurations = {
            no_change,
            [](Inputs& in) { in.mFSAAType = 2; },
            [](Inputs& in) { in.mFSAAType = 2; in.mSMAAStencil = false; },
            [](Inputs& in) { in.mShadowDetail = 0; in.mSSAO = false; },
            [](Inputs& in) { in.mResolutionDivisor = 3; in.mProbeSuperSample = 1; },
            [](Inputs& in) { in.mHDR = false; in.mGBufferNormHDR = false; in.mReverseZ = false; },
        };
        for (size_t c = 0; c < configurations.size(); ++c)
        {
            INFO("configuration " << c);
            Inputs in = defaults();
            configurations[c](in);
            const Plan plan = Plan::make(in);

            for (size_t i = 0; i < Plan::ID_COUNT; ++i)
            {
                const Plan::Resource& sharer = plan[(Id)i];
                if (!sharer.mDepthFrom)
                {
                    continue;
                }
                INFO(Plan::name((Id)i) << " from " << Plan::name(*sharer.mDepthFrom));
                REQUIRE(sharer.mDesc);
                const Plan::Resource& owner = plan[*sharer.mDepthFrom];
                REQUIRE(owner.mDesc);
                CHECK(!owner.mDepthFrom);
                CHECK(owner.mDesc->depth != Depth::None);
                CHECK(sharer.mDesc->depth == Depth::None);
                CHECK(!sharer.mDesc->stencil);
                CHECK(sharer.mDesc->width == owner.mDesc->width);
                CHECK(sharer.mDesc->height == owner.mDesc->height);
            }
        }
    }

    TEST_CASE("alrenderresourceplan: only the main view's render-sized targets drive the render size down", "[alrenderresourceplan]")
    {
        const IdSet shadow_maps = sun_shadows({ Pack::Main }) | of({ Id::SpotShadow0, Id::SpotShadow1 });
        for (size_t i = 0; i < Plan::ID_COUNT; ++i)
        {
            const Id id = (Id)i;
            INFO(Plan::name(id));
            CHECK(Plan::drivesRenderSize(id) == (Plan::followsRenderSize(id) && !shadow_maps.test(i)));
        }

        // The fixed-size targets, the capture packs and the textures never do, nor do the shadow maps: halving the
        // view would not make room for a square spot map sized from its width, nor for one with no size.
        for (Id id : { Id::ProbeGBuffer,
                       Id::ProbeSun0, Id::HeroGBuffer, Id::Noise, Id::SMAAArea, Id::MainSun0, Id::MainSun3,
                       Id::SpotShadow0, Id::SpotShadow1 })
        {
            INFO(Plan::name(id));
            CHECK(!Plan::drivesRenderSize(id));
        }
        for (Id id : { Id::MainGBuffer, Id::MainScreen })
        {
            INFO(Plan::name(id));
            CHECK(Plan::drivesRenderSize(id));
        }

        // The shadow maps are still sized from it, so a lower size builds them again smaller.
        for (Id id : { Id::MainSun0, Id::MainSun3, Id::SpotShadow0, Id::SpotShadow1 })
        {
            INFO(Plan::name(id));
            CHECK(Plan::followsRenderSize(id));
        }

        // Every pack's sun cascades and the spot pair, and nothing else.
        CHECK(names(Plan::idsWhere(Plan::isShadowMap)) ==
              names(sun_shadows({ Pack::Main, Pack::Probe, Pack::Hero }) | of({ Id::SpotShadow0, Id::SpotShadow1 })));
    }

    TEST_CASE("alrenderresourceplan: requested bloom shape preserves original scale clamps and one-texel stop",
              "[alrenderresourceplan][bloom-shape]")
    {
        Inputs in = defaults();
        constexpr U32 widths[] = {9, 6, 4, 2, 1};
        constexpr U32 heights[] = {5, 3, 2, 1, 1};
        constexpr U32 levels[] = {4, 3, 3, 2, 1};
        for (S32 scale = 0; scale < 5; ++scale)
        {
            CAPTURE(scale); in.mBloomScale = scale;
            const auto shape = in.bloomShape(9, 5);
            CHECK(shape.mWidth == widths[scale]);
            CHECK(shape.mHeight == heights[scale]);
            CHECK(shape.mMipCount == levels[scale]);
        }
        in.mBloomScale = -10;
        CHECK(in.bloomShape(9, 5).mMipCount == 4);
        in.mBloomScale = 99;
        CHECK(in.bloomShape(9, 5).mMipCount == 1);
        in.mBloomScale = 0; in.mBloomMips = -10;
        CHECK(in.bloomShape(64, 64).mMipCount == 3);
        in.mBloomMips = 99;
        CHECK(in.bloomShape(64, 64).mMipCount == 7);
        CHECK(in.bloomShape(1, 1).mMipCount == 1);
        CHECK(in.bloomShape(1, 16).mMipCount == 5);
        CHECK(in.bloomShape(16, 1).mMipCount == 5);
        for (const auto shape : {in.bloomShape(0, 16), in.bloomShape(16, 0), in.bloomShape(0, 0)})
        {
            CHECK(shape.mWidth == 0); CHECK(shape.mHeight == 0); CHECK(shape.mMipCount == 0);
        }
    }

    TEST_CASE("alrenderresourceplan: every resource has its own debugger label", "[alrenderresourceplan]")
    {
        CHECK(Plan::name(Id::MainGBuffer) == "main.gbuffer");
        CHECK(Plan::name(Id::ProbeSun1) == "probe.shadow.sun1");
        CHECK(Plan::name(Id::HeroSun3) == "hero.shadow.sun3");
        CHECK(Plan::name(Id::SpotShadow0) == "shadow.spot0");
        CHECK(Plan::name(Id::SMAASample) == "smaa.sample");

        std::set<std::string_view> seen;
        for (size_t i = 0; i < Plan::ID_COUNT; ++i)
        {
            const std::string_view name = Plan::name((Id)i);
            INFO("id " << i);
            CHECK(!name.empty());
            CHECK(seen.insert(name).second);
        }
    }
}
