/**
 * @file alpipeline_local_light_values_native.inl
 * @brief Actual owner point-light value gather with explicit viewer inputs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * PPD307: no GPU allocation, viewer object or renderer enters the result.
 */
#include <stdexcept>
#include "llmatrix4a.h"

namespace
{
namespace MainLightValuesTest
{
#include "../../newview/pipeline_local_light_packet_native.inl"
#include "../../newview/light_screen_rect_native.inl"

constexpr F32 DEFERRED_LIGHT_FALLOFF = .5f;
// The existing fade-clock conversion remains outside the production include.
// Supply its resolved brightness explicitly: these tests check multiplication
// and zero-light exclusion without making a second implementation of that clock.
F32 light_fade_brightness(F32 brightness) { return brightness; }

struct LLVOVolume
{
    bool attached = false, spotlight = false, forbid_reads = false;
    F32 radius = .125f, falloff = .5f;
    LLColor3 color{1.f, .5f, .25f};
    mutable U32 attachment_reads = 0, radius_reads = 0, color_reads = 0;
    mutable U32 spotlight_reads = 0, falloff_reads = 0;
    void checkRead() const
    {
        if (forbid_reads) throw std::logic_error("Unexpected filtered-light read");
    }
    bool isAttachment() const { checkRead(); ++attachment_reads; return attached; }
    F32 getLightRadius() const { checkRead(); ++radius_reads; return radius; }
    LLColor3 getLightLinearColor() const { checkRead(); ++color_reads; return color; }
    bool isLightSpotlight() const { checkRead(); ++spotlight_reads; return spotlight; }
    F32 getLightFalloff(F32 factor) const
    { checkRead(); ++falloff_reads; return falloff * factor; }
};
struct LLDrawable
{
    LLVOVolume* volume = nullptr;
    LLVector3 position;
    bool forbid_reads = false;
    mutable U32 volume_reads = 0, position_reads = 0;
    LLVOVolume* getVOVolume() const
    {
        if (forbid_reads) throw std::logic_error("Unexpected over-limit drawable read");
        ++volume_reads;
        return volume;
    }
    const LLVector3& getPositionAgent() const
    { ++position_reads; return position; }
};
// Match the production LLPointer conversion boundary without giving this
// handle ownership of stack/vector fixture objects.
struct DrawableReference
{
    LLDrawable* value = nullptr;
    DrawableReference(LLDrawable* input = nullptr) : value(input) {}
    operator LLDrawable*() const noexcept { return value; }
};
struct Light { DrawableReference drawable; F32 fade = 1.f; };
struct Camera
{
    LLVector3 origin{0.f, 0.f, 8.f};
    LLMatrix4a projection;
    std::vector<F32> hidden_x, tested_x, tested_radius;
    Camera() { projection.setIdentity(); }
    const LLVector3& getOrigin() const { return origin; }
    const LLMatrix4a& getProjection() const { return projection; }
    int AABBInFrustumNoFarClip(const LLVector4a& center, const LLVector4a& extent)
    {
        tested_x.push_back(center[0]);
        tested_radius.push_back(extent[0]);
        return std::find(hidden_x.begin(), hidden_x.end(), center[0]) == hidden_x.end();
    }
};
struct Owner
{
    using light_set_t = std::vector<Light>;
    light_set_t mNearbyLights;
    std::array<LLVOVolume, 16> volumes;
    std::array<LLDrawable, 16> drawables;
    std::size_t used = 0;
    Camera camera;
    LLMatrix4a modelview;
    MetalLocalLightValues values;
    S32 local_light_count = 256;
    F32 light_scale = 1.f;
    U32 light_width = 128, light_height = 128;
    bool sRenderAttachedLights = true;
    Owner() { modelview.setIdentity(); }
    std::size_t add(F32 x = 0.f, F32 y = 0.f, F32 z = 0.f)
    {
        if (used == volumes.size()) throw std::logic_error("Light fixture exhausted");
        const auto index = used++;
        drawables[index].volume = &volumes[index];
        drawables[index].position.set(x, y, z);
        mNearbyLights.push_back({&drawables[index], 1.f});
        return index;
    }
    void gather()
    {
#include "../../newview/pipeline_local_light_values_native.inl"
    }
};
}
}

TEST_CASE("Actual main light values consume the original limit before exclusions",
          "[main-light-values][almetal][pipeline-buffer-publication]")
{
    using namespace MainLightValuesTest;
    Owner owner;
    owner.mNearbyLights.push_back({nullptr, 1.f});
    const auto attachment = owner.add();
    owner.volumes[attachment].attached = true;
    owner.sRenderAttachedLights = false;
    const auto over_limit = owner.add(.5f);
    owner.drawables[over_limit].forbid_reads = true;
    owner.local_light_count = 2;
    REQUIRE_NOTHROW(owner.gather());
    CHECK(owner.values.lights.empty());
    CHECK(owner.values.bounded_count == 0);
    CHECK(owner.values.additive_pass_count == 2);
    CHECK(owner.drawables[attachment].position_reads == 0);
    CHECK(owner.volumes[attachment].radius_reads == 0);
    CHECK(owner.drawables[over_limit].volume_reads == 0);
    CHECK(owner.camera.tested_x.empty());

    for (const S32 limit : {0, -1})
    {
        owner.local_light_count = limit;
        owner.drawables[attachment].forbid_reads = true;
        REQUIRE_NOTHROW(owner.gather());
        CHECK(owner.values.lights.empty());
        CHECK(owner.values.additive_pass_count == 0);
        CHECK(owner.values.bounded_count == 0);
    }
}

TEST_CASE("Actual main light values retain point-light filters and avoid excluded later reads",
          "[main-light-values][almetal][pipeline-buffer-publication]")
{
    using namespace MainLightValuesTest;
    Owner owner;
    const auto absent_volume = owner.add();
    owner.drawables[absent_volume].volume = nullptr;
    owner.volumes[absent_volume].forbid_reads = true;
    const auto attachment = owner.add(.125f);
    owner.volumes[attachment].attached = true;
    owner.sRenderAttachedLights = false;
    const auto spotlight = owner.add(.25f);
    owner.volumes[spotlight].spotlight = true;
    const auto hidden = owner.add(.375f);
    owner.camera.hidden_x.push_back(.375f);
    const auto dim = owner.add(.5f);
    owner.volumes[dim].color = LLColor3(0.f, 0.f, 0.f);
    const auto radius = owner.add(.625f);
    owner.volumes[radius].radius = 0.f;
    const auto faded = owner.add(.75f);
    owner.mNearbyLights.back().fade = 0.f;
    const auto accepted = owner.add(.875f);
    REQUIRE_NOTHROW(owner.gather());
    REQUIRE(owner.values.lights.size() == 1);
    CHECK(owner.values.lights[0].position_radius[0] == .875f);
    CHECK(owner.values.bounded_count == 1);
    CHECK(owner.values.additive_pass_count == 8);
    CHECK(owner.drawables[absent_volume].position_reads == 0);
    CHECK(owner.drawables[attachment].position_reads == 0);
    CHECK(owner.volumes[attachment].radius_reads == 0);
    CHECK(owner.volumes[spotlight].falloff_reads == 0);
    CHECK(owner.volumes[hidden].spotlight_reads == 0);
    CHECK(owner.volumes[hidden].falloff_reads == 0);
    CHECK(owner.volumes[dim].spotlight_reads == 0);
    CHECK(owner.volumes[radius].spotlight_reads == 0);
    CHECK(owner.volumes[faded].spotlight_reads == 0);
    CHECK(owner.camera.tested_x == std::vector<F32>{.25f, .375f, .875f});
    CHECK(owner.camera.tested_radius == std::vector<F32>{.1875f, .1875f, .1875f});
    CHECK(owner.volumes[accepted].falloff_reads == 1);

    owner.sRenderAttachedLights = true;
    REQUIRE_NOTHROW(owner.gather());
    REQUIRE(owner.values.lights.size() == 2);
    CHECK(owner.values.lights[0].position_radius[0] == .125f);
    CHECK(owner.values.lights[1].position_radius[0] == .875f);
    CHECK(owner.values.inside_lights.empty());
}

TEST_CASE("Actual main light values preserve bounded then inside order and copied numeric ownership",
          "[main-light-values][almetal][pipeline-buffer-publication]")
{
    using namespace MainLightValuesTest;
    MetalLocalLightValues retained;
    {
        Owner owner;
        owner.camera.origin.set(0.f, 0.f, 0.f);
        owner.add(0.f);
        owner.add(.75f);
        owner.add(.0625f);
        owner.add(-.75f);
        REQUIRE_NOTHROW(owner.gather());
        REQUIRE(owner.values.lights.size() == 4);
        CHECK(owner.values.bounded_count == 2);
        // The limited prefix reserves four entries in EACH array. The final
        // bounded+inside merge uses at most four, without a transient grow.
        CHECK(owner.values.lights.capacity() == 4);
        CHECK(owner.values.inside_lights.capacity() == 4);
        CHECK(owner.values.lights[0].position_radius[0] == .75f);
        CHECK(owner.values.lights[1].position_radius[0] == -.75f);
        CHECK(owner.values.lights[2].position_radius[0] == 0.f);
        CHECK(owner.values.lights[3].position_radius[0] == .0625f);
        CHECK(owner.values.lights[2].screen_rect[2] == 0);
        CHECK(owner.values.lights[3].screen_rect[3] == 0);
        CHECK(owner.values.inside_lights.empty());
        retained = std::move(owner.values);
        owner.drawables[1].position.set(9.f, 9.f, 9.f);
        owner.volumes[1].color = LLColor3(0.f, 0.f, 0.f);
    }
    REQUIRE(retained.lights.size() == 4);
    CHECK(retained.lights[0].position_radius[0] == .75f);
    CHECK(retained.lights[0].color_falloff[0] == 1.f);
    CHECK(retained.lights[0].color_falloff[1] == .5f);
    CHECK(retained.lights[0].color_falloff[2] == .25f);
    CHECK(retained.bounded_count == 2);
}

TEST_CASE("Actual main light values use copied view dimensions and production conservative screen rectangles",
          "[main-light-values][almetal][pipeline-buffer-publication]")
{
    using namespace MainLightValuesTest;
    Owner owner;
    owner.add(.3125f, -.3125f, 0.f);
    owner.modelview.setTranslation(LLVector3(.25f, .125f, -.5f));
    owner.light_scale = 2.f;
    owner.mNearbyLights[0].fade = .25f;
    REQUIRE_NOTHROW(owner.gather());
    REQUIRE(owner.values.lights.size() == 1);
    const auto& light = owner.values.lights[0];
    CHECK(light.position_radius[0] == .5625f);
    CHECK(light.position_radius[1] == -.1875f);
    CHECK(light.position_radius[2] == -.5f);
    CHECK(light.position_radius[3] == .1875f);
    CHECK(light.color_falloff[0] == .5f);
    CHECK(light.color_falloff[1] == .25f);
    CHECK(light.color_falloff[2] == .125f);
    CHECK(light.color_falloff[3] == .25f);
    CHECK(light.screen_rect[0] == 87);
    CHECK(light.screen_rect[1] == 39);
    CHECK(light.screen_rect[2] == 26);
    CHECK(light.screen_rect[3] == 26);


    owner.light_width = 0;
    owner.light_height = 17;
    REQUIRE_NOTHROW(owner.gather());
    REQUIRE(owner.values.lights.size() == 1);
    CHECK(owner.values.lights[0].screen_rect[0] == 0);
    CHECK(owner.values.lights[0].screen_rect[1] == 0);
    CHECK(owner.values.lights[0].screen_rect[2] == 0);
    CHECK(owner.values.lights[0].screen_rect[3] == 17);
}
