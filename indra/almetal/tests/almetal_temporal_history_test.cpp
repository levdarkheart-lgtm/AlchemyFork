/**
 * @file almetal_temporal_history_test.cpp
 * @brief Main-world submission history and analytical camera/instance pose contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetaltemporalworldhistory.h"
#include "almetalauroraconventions.h"
#include "alnativepublication.h"
#include "almetalworldmaterialdata.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace
{
using History = ALMetalTemporalWorldHistory;
using Catch::Approx;
void identity(float* out)
{ std::fill_n(out, 16, 0.f); out[0] = out[5] = out[10] = out[15] = 1.f; }
History::FrameInput input(std::uint64_t id)
{
    History::FrameInput in;
    in.world_id = id; in.input_width = 64; in.input_height = 48;
    in.output_width = 128; in.output_height = 96; in.mode = 5;
    identity(in.camera_mvp);
    return in;
}
ALMetalWorldSimpleBatch batch(std::uint64_t id)
{
    ALMetalWorldSimpleBatch b;
    b.temporal_instance_id = id; b.temporal_generation = 1;
    b.vertex_count = 3; b.index_count = 3;
    identity(b.modelview_projection);
    return b;
}
ALMetalWorldGBufferScene scene(const ALMetalWorldSimpleBatch* batches, std::size_t count)
{
    ALMetalWorldGBufferScene opaque;
    opaque.batches = batches; opaque.batch_count = count;
    return opaque;
}
}

TEST_CASE("Temporal projection jitter shifts raster pixels with unchanged depth and homogeneous W", "[temporal-history]")
{
    std::array<float,16> matrix{}; identity(matrix.data());
    matrix[3] = .5f; matrix[7] = .25f; matrix[11] = -1.f;
    const auto original = matrix;
    const std::array<float,4> point{2.f,3.f,-4.f,1.f};
    const auto project = [&](const auto& m)
    {
        std::array<float,4> clip{};
        for (unsigned r = 0; r != 4; ++r)
            for (unsigned c = 0; c != 4; ++c) clip[r] += m[c * 4 + r] * point[c];
        return clip;
    };
    const auto before = project(matrix);
    alMetalApplyTemporalJitter(matrix.data(), .25f, -.3f, 100, 80);
    const auto after = project(matrix);
    CHECK((after[0] / after[3] - before[0] / before[3]) * 50.f == Approx(.25f).margin(1.e-5f));
    CHECK((after[1] / after[3] - before[1] / before[3]) * 40.f == Approx(-.3f).margin(1.e-5f));
    CHECK(after[2] == before[2]); CHECK(after[3] == before[3]);
    CHECK(original[0] == 1.f); // Caller retained its logical camera copy.
    const auto jittered = matrix;
    alMetalApplyTemporalJitter(matrix.data(), .25f, -.3f, 0, 80);
    CHECK(matrix == jittered);
}

TEST_CASE("Temporal culling projection includes the full jitter guard band without changing depth", "[temporal-history]")
{
    std::array<float,16> projection{}; identity(projection.data());
    const auto original = projection;
    alMetalExpandTemporalCullProjection(projection.data(), 100, 80);
    CHECK(projection[0] * (1.f + 2.f / 100.f) == Approx(1.f));
    CHECK(projection[5] * (1.f + 2.f / 80.f) == Approx(1.f));
    CHECK(projection[0] * (1.f + 1.f / 100.f) < 1.f);
    CHECK(projection[5] * (1.f + 1.f / 80.f) < 1.f);
    CHECK(projection[10] == original[10]); CHECK(projection[15] == original[15]);
    CHECK(original[0] == 1.f); CHECK(original[5] == 1.f);
}

TEST_CASE("Temporal history advances jitter and poses only at successful world submission", "[temporal-history]")
{
    History history;
    auto b = batch(41);
    auto opaque = scene(&b, 1);
    REQUIRE(history.beginFrame(input(1)));
    CHECK(history.resetRequired());
    CHECK(history.jitterPixels()[0] == 0.f);
    CHECK(history.jitterPixels()[1] == Approx(-1.f / 6.f));
    REQUIRE(history.stage(opaque, nullptr));
    REQUIRE(history.previousOpaque().size() == 1);
    CHECK_FALSE(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].reactive);
    CHECK(history.committedWorld() == 0);
    CHECK_FALSE(history.commit(2));
    CHECK(history.pendingWorld() == 1);
    REQUIRE(history.commit(1));
    CHECK(history.committedWorld() == 1);
    CHECK(history.stats().submitted_worlds == 1);
    CHECK(history.stats().resets == 1);

    REQUIRE(history.beginFrame(input(2)));
    CHECK_FALSE(history.resetRequired());
    const auto jitter = history.jitterPixels();
    CHECK(jitter[0] == -.25f);
    CHECK(jitter[1] == Approx(1.f / 6.f));
    b.modelview_projection[12] = .25f;
    REQUIRE(history.stage(opaque, nullptr));
    REQUIRE(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].modelview_projection[12] == 0.f);
    history.abandon(999); // An unrelated transaction cannot drop this one.
    CHECK(history.pendingWorld() == 2);
    history.abandon(2);
    CHECK(history.committedWorld() == 1);
    CHECK(history.stats().submitted_worlds == 1);
    REQUIRE(history.beginFrame(input(3)));
    CHECK(history.jitterPixels() == jitter);
    REQUIRE(history.stage(opaque, nullptr));
    CHECK(history.previousOpaque()[0].modelview_projection[12] == 0.f);
    REQUIRE(history.commit(3));
    CHECK(history.stats().submitted_worlds == 2);
    CHECK(history.stats().abandoned_worlds == 1);
}

TEST_CASE("Temporal jitter cycle follows rounded upscale extents and only submitted worlds", "[temporal-history]")
{
    struct Case { std::uint32_t input_width,input_height,output_width,output_height,mode,phases; };
    for (const auto& c : {Case{1728,1084,3456,2168,5,32},Case{2304,1446,3456,2168,6,18},
         Case{2592,1626,3456,2168,7,15},Case{1729,1085,3457,2169,5,32},
         Case{2305,1446,3457,2169,6,18},Case{2593,1627,3457,2169,7,15},
         Case{1,4,1,7,5,25}})
    {
        History history;
        auto in = input(1);
        in.input_width=c.input_width;in.input_height=c.input_height;
        in.output_width=c.output_width;in.output_height=c.output_height;in.mode=c.mode;
        CHECK(History::recommendedJitterPhaseCount(c.input_width,c.input_height,c.output_width,c.output_height)==c.phases);
        std::array<float,2> first{};
        ALMetalWorldGBufferScene empty;
        for (std::uint32_t phase=0;phase<c.phases;++phase)
        {
            in.world_id=phase+1;
            REQUIRE(history.beginFrame(in));
            CHECK(history.jitterPhaseCount()==c.phases);
            CHECK(history.stats().jitter_phase==phase);
            CHECK(history.stats().jitter_phase_count==c.phases);
            if (!phase) first=history.jitterPixels();
            REQUIRE(history.stage(empty,nullptr)); REQUIRE(history.commit(in.world_id));
        }
        CHECK(history.stats().submitted_worlds==c.phases);
        in.world_id=c.phases+1;
        REQUIRE(history.beginFrame(in));
        CHECK(history.stats().jitter_phase==0);
        CHECK(history.jitterPixels()==first);
        history.abandon();
        CHECK(history.stats().submitted_worlds==c.phases);
        in.world_id=c.phases+2;
        REQUIRE(history.beginFrame(in));
        CHECK(history.stats().jitter_phase==0);
        CHECK(history.jitterPixels()==first);
        history.abandon();
        // A different requested scale resets the sequence even when rounded
        // extents happen to be identical at a very small viewport.
        in.world_id=c.phases+3;in.mode=c.mode==5?6:5;
        REQUIRE(history.beginFrame(in));
        CHECK(history.resetRequired());CHECK(history.stats().jitter_phase==0);
    }
    CHECK(History::recommendedJitterPhaseCount(1,1,3,3)==72);
    CHECK(History::recommendedJitterPhaseCount(1,1,100,100)==128);
    CHECK(History::recommendedJitterPhaseCount(0,1,2,2)==0);
    CHECK(History::recommendedJitterPhaseCount(2,1,1,2)==0);
}

TEST_CASE("Aurora phase policy shares submission-only pose history and resets on policy change", "[temporal-history][aurora]")
{
    History history;
    auto in=input(1);
    in.input_width=2305; in.input_height=1446;
    in.output_width=3457; in.output_height=2169; in.mode=8;
    in.jitter_phase_count=17;
    ALMetalWorldGBufferScene empty;
    std::array<float,2> first{};
    for (std::uint32_t phase=0;phase<17;++phase)
    {
        in.world_id=phase+1;
        REQUIRE(history.beginFrame(in));
        CHECK(history.jitterPhaseCount()==17);
        CHECK(history.stats().jitter_phase==phase);
        if (!phase) first=history.jitterPixels();
        REQUIRE(history.stage(empty,nullptr)); REQUIRE(history.commit(in.world_id));
    }
    in.world_id=18;
    REQUIRE(history.beginFrame(in));
    CHECK(history.jitterPixels()==first);
    history.abandon();
    CHECK(history.committedWorld()==17);
    in.world_id=19; REQUIRE(history.beginFrame(in));
    CHECK(history.jitterPixels()==first); history.abandon();
    // Same dimensions, but a different reconstruction phase contract: reset.
    in.world_id=20; in.jitter_phase_count=18;
    REQUIRE(history.beginFrame(in)); CHECK(history.resetRequired());
    CHECK(history.stats().jitter_phase==0); CHECK(history.jitterPhaseCount()==18);
}

TEST_CASE("Temporal camera reprojection has analytical sign and preserves independent object motion", "[temporal-history]")
{
    History history;
    auto b = batch(7); auto opaque = scene(&b, 1);
    REQUIRE(history.beginFrame(input(1)));
    REQUIRE(history.stage(opaque, nullptr));
    REQUIRE(history.commit(1));
    auto current = input(2);
    current.camera_mvp[12] = .25f;
    b.modelview_projection[12] = .75f; // Camera+.25 and rigid object+.5.
    REQUIRE(history.beginFrame(current));
    CHECK_FALSE(history.resetRequired());
    CHECK(history.clipToPrevious()[12] == Approx(-.25f));
    // Original +clip Y is +texture Y. A camera +.125 clip-Y translation
    // maps a current point back by -.125, independently of viewport size.
    CHECK(history.clipToPrevious()[13] == 0.f);
    REQUIRE(history.stage(opaque, nullptr));
    CHECK(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].modelview_projection[12] == 0.f);
    CHECK(b.modelview_projection[12] == .75f);
    REQUIRE(history.commit(2));
    current = input(3); current.camera_mvp[13] = .125f;
    REQUIRE(history.beginFrame(current));
    CHECK(history.clipToPrevious()[12] == Approx(.25f));
    CHECK(history.clipToPrevious()[13] == Approx(-.125f));
}

TEST_CASE("Temporal palettes are owned snapshots of the preceding submitted pose", "[temporal-history]")
{
    History history;
    auto b = batch(9); b.avatar_skinning = true;
    std::array<float, 180> palette{};
    for (std::size_t i = 0; i != palette.size(); ++i) palette[i] = static_cast<float>(i) / 180.f;
    b.skin_palette = palette.data(); b.skin_palette_float_count = palette.size(); b.skin_joint_count = 15;
    auto opaque = scene(&b, 1);
    REQUIRE(history.beginFrame(input(1)));
    REQUIRE(history.stage(opaque, nullptr));
    palette[40] = 2.f; // Simulation advances after capture but before submission.
    REQUIRE(history.commit(1));
    REQUIRE(history.beginFrame(input(2)));
    REQUIRE(history.stage(opaque, nullptr));
    const auto& previous = history.previousOpaque()[0];
    REQUIRE(previous.valid);
    CHECK(previous.skin_joint_count == 15);
    REQUIRE(previous.skin_palette.size() == 180);
    CHECK(previous.skin_palette[40] == Approx(40.f / 180.f));
    palette[40] = 3.f;
    CHECK(previous.skin_palette[40] == Approx(40.f / 180.f));
    REQUIRE(history.commit(2));
    REQUIRE(history.beginFrame(input(3)));
    REQUIRE(history.stage(opaque, nullptr));
    CHECK(history.previousOpaque()[0].skin_palette[40] == 2.f);
    history.abandon();
    CHECK(history.stats().committed_draws == 1);
}

TEST_CASE("Temporal identity generation topology and duplicate ambiguity reject unrelated history", "[temporal-history]")
{
    for (unsigned change = 0; change != 5; ++change)
    {
        History history;
        std::array<ALMetalWorldSimpleBatch, 2> batches{batch(10), batch(11)};
        auto opaque = scene(batches.data(), 1);
        REQUIRE(history.beginFrame(input(1)));
        REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
        switch (change)
        {
            case 0: ++batches[0].temporal_generation; break;
            case 1: ++batches[0].temporal_instance_id; break;
            case 2: ++batches[0].index_count; break;
            case 3: batches[0].position_offset = 64; break;
            case 4: batches[1] = batches[0]; opaque.batch_count = 2; break;
        }
        REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, nullptr));
        CHECK_FALSE(history.previousOpaque()[0].valid);
        CHECK(history.previousOpaque()[0].reactive);
        if (change == 4)
        {
            CHECK_FALSE(history.previousOpaque()[1].valid);
            REQUIRE(history.commit(2));
            opaque.batch_count = 1;
            REQUIRE(history.beginFrame(input(3))); REQUIRE(history.stage(opaque, nullptr));
            CHECK_FALSE(history.previousOpaque()[0].valid);
        }
    }
}

TEST_CASE("Temporal raw avatar poses survive transient capture storage relocation", "[temporal-history][metal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    REQUIRE(device);
    ALMetalBufferDescriptor descriptor;
    descriptor.length = 256;
    auto vertices = ALMetalBuffer::create(*device, descriptor, &error);
    REQUIRE(vertices);
    History history;
    auto first = std::make_unique<ALMetalWorldSimpleBatch>(batch(0));
    first->vertices = vertices.get(); first->temporal_generation = 0; first->avatar_skinning = true;
    std::array<float,180> palette{};
    palette[0] = 1.f;
    first->skin_palette = palette.data(); first->skin_palette_float_count = palette.size(); first->skin_joint_count = 15;
    auto opaque = scene(first.get(),1);
    REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque,nullptr)); REQUIRE(history.commit(1));
    auto relocated = std::make_unique<ALMetalWorldSimpleBatch>(*first);
    REQUIRE(relocated.get() != first.get());
    relocated->modelview_projection[12] = .2f;
    palette[0] = .75f;
    opaque.batches = relocated.get();
    REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque,nullptr));
    REQUIRE(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].modelview_projection[12] == 0.f);
    CHECK(history.previousOpaque()[0].skin_palette[0] == 1.f);
    REQUIRE(history.commit(2));
    auto replacement_vertices = ALMetalBuffer::create(*device,descriptor,&error);
    REQUIRE(replacement_vertices);
    relocated->vertices = replacement_vertices.get();
    REQUIRE(history.beginFrame(input(3))); REQUIRE(history.stage(opaque,nullptr));
    CHECK_FALSE(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].reactive);
}

TEST_CASE("Temporal published vertex rewrites reject old poses even with unchanged native allocation generation", "[temporal-history][metal]")
{
    std::string error;
    auto device=ALMetalDevice::create(&error); REQUIRE(device);
    ALMetalBufferDescriptor descriptor;descriptor.length=256;
    auto vertices=ALMetalBuffer::create(*device,descriptor,&error); REQUIRE(vertices);
    ALNativePublication::Signal publication;
    publication.didPublish();
    auto b=batch(publication.identity());
    b.vertices=vertices.get();b.temporal_generation=publication.publishedRevision();
    const auto allocation_identity=vertices->resourceView().identity();
    const auto allocation_generation=vertices->resourceView().generation();
    const auto published_revision=publication.publishedRevision();
    History history;auto opaque=scene(&b,1);
    REQUIRE(history.beginFrame(input(1)));REQUIRE(history.stage(opaque,nullptr));REQUIRE(history.commit(1));
    // The actual native buffer supports in-place writes after its prior use
    // retires. Its allocation ticket is not a vertex-content publication.
    publication.willWrite();
    const std::array<float,4> changed_position{.25f,.5f,.75f,1.f};
    REQUIRE(vertices->write(0,changed_position.data(),sizeof(changed_position),&error));
    publication.didPublish();
    CHECK(vertices->resourceView().identity()==allocation_identity);
    CHECK(vertices->resourceView().generation()==allocation_generation);
    REQUIRE(publication.publishedRevision()!=published_revision);
    // Negative control: leaving the old producer stamp would still match.
    REQUIRE(history.beginFrame(input(2)));REQUIRE(history.stage(opaque,nullptr));
    CHECK(history.previousOpaque()[0].valid);
    history.abandon();
    // The production capture copies this existing producer publication stamp.
    b.temporal_generation=publication.publishedRevision();
    REQUIRE(history.beginFrame(input(3)));REQUIRE(history.stage(opaque,nullptr));
    CHECK_FALSE(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].reactive);
}

TEST_CASE("Temporal descriptors cuts origin changes and recovery reset without an ordinary motion reset", "[temporal-history]")
{
    for (unsigned change = 0; change != 8; ++change)
    {
        History history; auto b = batch(1); auto opaque = scene(&b, 1);
        REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
        auto next = input(2);
        switch (change)
        {
            case 0: next.input_width = 65; break;
            case 1: next.output_height = 100; break;
            case 2: next.mode = 6; break;
            case 3: next.origin[0] = 256.; break;
            case 4: next.force_reset = true; break;
            case 5: next.reverse_z = false; break;
            case 6: history.invalidate(); break;
            case 7: next.camera_mvp[12] = .1f; break;
        }
        REQUIRE(history.beginFrame(next));
        CHECK(history.resetRequired() == (change != 7));
        REQUIRE(history.stage(opaque, nullptr));
        CHECK(history.previousOpaque()[0].valid == (change == 7));
        history.abandon();
        // An abandoned resized/cut frame did not replace the old camera/pose.
        if (change != 6)
        {
            REQUIRE(history.beginFrame(input(3))); CHECK_FALSE(history.resetRequired());
            REQUIRE(history.stage(opaque, nullptr)); CHECK(history.previousOpaque()[0].valid);
        }
    }
}

TEST_CASE("Temporal foreground UV changes are reactive and forward history stays family isolated", "[temporal-history]")
{
    History history;
    auto b = batch(71); b.alpha_mask = true;
    auto f = batch(71); f.pre_water_alpha = true;
    auto opaque = scene(&b, 1);
    ALMetalWorldOpaquePostDeferredScene forward;
    forward.batches = &f; forward.batch_count = 1;
    f.modelview_projection[12] = .5f;
    REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, forward)); REQUIRE(history.commit(1));
    b.texture_matrix[12] = .3f;
    REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, forward));
    CHECK(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].reactive);
    CHECK(history.previousOpaque()[0].texture_matrix[12] == 0.f);
    CHECK(history.previousOpaque()[0].modelview_projection[12] == 0.f);
    CHECK(history.previousForward()[0].valid);
    CHECK(history.previousForward()[0].modelview_projection[12] == .5f);
    const auto before = history.stats().submitted_worlds;
    // Independent capture/UI owners invoke no begin/commit: staging mutable
    // simulation or presentation counters alone cannot advance this sequence.
    b.modelview_projection[12] = 1.f;
    CHECK(history.stats().submitted_worlds == before);
    CHECK(history.committedWorld() == 1);
}

TEST_CASE("Temporal glTF material UV animation cannot inherit static coverage history", "[temporal-history]")
{
    History history; auto b = batch(17); b.pbr_slot_count = 1;
    auto opaque = scene(&b, 1);
    REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
    b.pbrSlots()[0].texture_transform[0][2] = .25f;
    REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, nullptr));
    CHECK_FALSE(history.previousOpaque()[0].valid);
    CHECK(history.previousOpaque()[0].reactive);
}

TEST_CASE("Temporal equal material republications preserve history without retaining proof identity", "[temporal-history][metal][material-semantics]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    ALMetalWorldMaterialDataCache cache(*device);
    ALMetalTextureDescriptor descriptor; descriptor.width = descriptor.height = 4;
    auto image = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(image);
    for (unsigned family : {1u, 2u})
    {
        CAPTURE(family);
        auto authored = batch(451); authored.texture_count = 1; authored.diffuse_textures[0] = image.get();
        if (family == 1) { authored.material_slot_count = 1; authored.materialSlots()[0].alpha_cutoff = .4f; }
        else
        {
            authored.pbr_slot_count = 1; authored.alpha_mask = true;
            auto& slot = authored.pbrSlots()[0]; slot.alpha_cutoff = .4f;
            slot.normal_texture = slot.orm_texture = slot.emissive_texture = image.get();
        }
        auto first = authored, second = authored;
        auto first_source = ALMetalWorldMaterialSource::publish(*device, first, cache, &error); INFO(error); REQUIRE(first_source);
        auto second_source = ALMetalWorldMaterialSource::publish(*device, second, cache, &error); INFO(error); REQUIRE(second_source);
        REQUIRE(first_source != second_source);
        CHECK((family == 1 ? first_source->materialLease() : first_source->pbrLease(true)) ==
              (family == 1 ? second_source->materialLease() : second_source->pbrLease(true)));
        History history;
        auto opaque = scene(&first, 1);
        REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
        const std::weak_ptr<const ALMetalWorldMaterialSource> weak_first = first_source;
        first.native_material_source.reset(); first_source.reset(); // History owns values, not this transient proof.
        CHECK(weak_first.expired());
        opaque.batches = &second;
        REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, nullptr));
        CHECK(history.previousOpaque()[0].valid); CHECK_FALSE(history.previousOpaque()[0].reactive);
        CHECK(history.stats().valid_previous_draws == 1);
        REQUIRE(history.commit(2));
        // Returning to equal raw authored values also preserves the same material.
        opaque.batches = &authored;
        REQUIRE(history.beginFrame(input(3))); REQUIRE(history.stage(opaque, nullptr));
        CHECK(history.previousOpaque()[0].valid); CHECK_FALSE(history.previousOpaque()[0].reactive);
    }
}

TEST_CASE("Temporal material semantics reject independent authored channel and sampling changes", "[temporal-history][metal][material-semantics]")
{
    std::string error; auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    ALMetalTextureDescriptor descriptor; descriptor.width = descriptor.height = 4;
    auto image = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(image);
    auto replacement = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(replacement);
    for (unsigned family : {0u, 1u, 2u}) for (unsigned change = 0; change != (family == 2 ? 18u : family == 1 ? 11u : 7u); ++change)
    {
        CAPTURE(family, change);
        auto b = batch(452); b.texture_count = 1; b.diffuse_textures[0] = image.get();
        if (family == 1) b.material_slot_count = 1;
        if (family == 2) b.pbr_slot_count = 1;
        History history; auto opaque = scene(&b, 1);
        REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
        if (change == 0) b.diffuse_textures[0] = replacement.get();
        else if (change == 1) b.use_vertex_color = !b.use_vertex_color;
        else if (change == 2) b.alpha_cutoff_bias = !b.alpha_cutoff_bias;
        else if (change == 3) b.pre_water_minimum_alpha = .1f;
        else if (change == 4) b.pre_water_destination_rgb = ALMetalBlendFactor::one;
        else if (family == 0)
        {
            if (change == 5) b.normal_texture = replacement.get();
            else b.avatar_diffuse_srgb = !b.avatar_diffuse_srgb;
        }
        else if (family == 1)
        {
            auto& slot = b.materialSlots()[0];
            switch (change)
            {
                case 5: slot.normal_texture = replacement.get(); break;
                case 6: slot.specular_texture = replacement.get(); break;
                case 7: slot.specular_color[3] = .2f; break;
                case 8: slot.env_intensity = .3f; break;
                case 9: slot.alpha_cutoff = .7f; break;
                case 10: slot.fullbright = .4f; break;
            }
        }
        else
        {
            auto& slot = b.pbrSlots()[0];
            switch (change)
            {
                case 5: slot.normal_texture = replacement.get(); break;
                case 6: slot.orm_texture = replacement.get(); break;
                case 7: slot.emissive_texture = replacement.get(); break;
                case 8: slot.base_color_factor[0] = .2f; break;
                case 9: slot.base_color_factor[3] = .3f; break;
                case 10: slot.texture_transform[0][2] = .1f; break;
                case 11: slot.texture_transform[1][1] = .2f; break;
                case 12: slot.texture_transform[2][4] = .3f; break;
                case 13: slot.texture_transform[3][5] = .4f; break;
                case 14: slot.emissive_color[2] = .5f; break;
                case 15: slot.roughness_factor = .6f; break;
                case 16: slot.metallic_factor = .7f; break;
                case 17: slot.alpha_cutoff = .8f; break;
            }
        }
        REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, nullptr));
        CHECK_FALSE(history.previousOpaque()[0].valid); CHECK(history.previousOpaque()[0].reactive);
    }
}

TEST_CASE("Temporal PBR transform history tracks used lanes without interpreting reserved packing", "[temporal-history][material-semantics]")
{
    for (unsigned channel = 0; channel != 4; ++channel) for (unsigned lane = 0; lane != 8; ++lane)
    {
        CAPTURE(channel, lane);
        History history; auto b = batch(457); b.pbr_slot_count = 1;
        auto opaque = scene(&b, 1);
        REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
        b.pbrSlots()[0].texture_transform[channel][lane] = .25f;
        REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, nullptr));
        const bool consumed = lane == 0 || lane == 1 || lane == 2 || lane == 4 || lane == 5;
        CHECK(history.previousOpaque()[0].valid == !consumed);
        CHECK(history.previousOpaque()[0].reactive == consumed);
    }
}

TEST_CASE("Temporal main material mip-policy changes reject prior coverage history", "[temporal-history][material-semantics]")
{
    for (bool forward_changed : {false, true})
    {
        History history; auto b = batch(454), f = batch(455);
        auto opaque = scene(&b, 1); opaque.material_mip_bias = -1.5f;
        ALMetalWorldOpaquePostDeferredScene forward; forward.batches = &f; forward.batch_count = 1;
        forward.material_mip_bias = opaque.material_mip_bias;
        REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, forward)); REQUIRE(history.commit(1));
        if (forward_changed) forward.material_mip_bias = -1.f; else opaque.material_mip_bias = -1.f;
        REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, forward));
        CHECK(history.previousOpaque()[0].valid == forward_changed);
        CHECK(history.previousForward()[0].valid != forward_changed);
    }
}

TEST_CASE("Temporal texture wrapper aliases preserve history and pooled generation changes reject it", "[temporal-history][metal][material-semantics]")
{
    std::string error; auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    auto pool = ALMetalRenderTargetPool::create(*device, 1024 * 1024); REQUIRE(pool);
    ALMetalRenderTargetDescriptor descriptor; descriptor.width = descriptor.height = 4;
    descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    auto target = pool->acquire(descriptor, &error); INFO(error); REQUIRE(target);
    const auto identity = target->colorTexture(0)->resourceView().identity();
    const auto generation = target->colorTexture(0)->resourceView().generation();
    auto alias = ALMetalTexture::retainSharedBacking(*target->colorTexture(0), &error); REQUIRE(alias);
    auto b = batch(453); b.texture_count = 1; b.diffuse_textures[0] = target->colorTexture(0);
    History history; auto opaque = scene(&b, 1);
    REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
    b.diffuse_textures[0] = alias.get();
    REQUIRE(history.beginFrame(input(2))); REQUIRE(history.stage(opaque, nullptr));
    CHECK(history.previousOpaque()[0].valid); REQUIRE(history.commit(2));
    alias.reset(); b.diffuse_textures[0] = nullptr; target.reset();
    auto reused = pool->acquire(descriptor, &error); REQUIRE(reused);
    REQUIRE(reused->colorTexture(0)->resourceView().identity() == identity);
    REQUIRE(reused->colorTexture(0)->resourceView().generation() > generation);
    b.diffuse_textures[0] = reused->colorTexture(0);
    REQUIRE(history.beginFrame(input(3))); REQUIRE(history.stage(opaque, nullptr));
    CHECK_FALSE(history.previousOpaque()[0].valid); CHECK(history.previousOpaque()[0].reactive);
}

TEST_CASE("Temporal canonical material payloads consume the existing bounded frame allowance", "[temporal-history][material-semantics]")
{
    auto b = batch(456); b.pbr_slot_count = b.max_material_slots;
    auto opaque = scene(&b, 1); History measured;
    REQUIRE(measured.beginFrame(input(1))); REQUIRE(measured.stage(opaque, nullptr)); REQUIRE(measured.commit(1));
    const auto required = measured.stats().committed_bytes;
    REQUIRE(required > 0);
    History insufficient({1, required - 1}); std::string error;
    REQUIRE(insufficient.beginFrame(input(1)));
    CHECK_FALSE(insufficient.stage(opaque, nullptr, &error));
    CHECK(error.find("material snapshots") != std::string::npos);
    CHECK_FALSE(insufficient.pending()); CHECK(insufficient.committedWorld() == 0);
    History exact({1, required});
    REQUIRE(exact.beginFrame(input(1))); REQUIRE(exact.stage(opaque, nullptr)); REQUIRE(exact.commit(1));
    CHECK(exact.stats().committed_bytes == required);
}

TEST_CASE("Temporal history bounds malformed input and frame storage without accepting partial snapshots", "[temporal-history]")
{
    std::string error;
    History history({1, 1024u * 1024u});
    std::array<ALMetalWorldSimpleBatch,2> batches{batch(1),batch(2)};
    auto opaque = scene(batches.data(), 1);
    auto invalid = input(1); invalid.camera_mvp[0] = std::numeric_limits<float>::infinity();
    CHECK_FALSE(history.beginFrame(invalid, &error));
    CHECK_FALSE(history.pending());
    invalid = input(1); std::fill_n(invalid.camera_mvp, 16, 0.f);
    CHECK_FALSE(history.beginFrame(invalid, &error));
    REQUIRE(history.beginFrame(input(1))); REQUIRE(history.stage(opaque, nullptr)); REQUIRE(history.commit(1));
    REQUIRE(history.beginFrame(input(2))); opaque.batch_count = 2;
    CHECK_FALSE(history.stage(opaque, nullptr, &error));
    CHECK_FALSE(history.pending()); CHECK(history.committedWorld() == 0);
    opaque.batch_count = 1;
    REQUIRE(history.beginFrame(input(3))); CHECK(history.resetRequired());
    history.abandon();
    History bytes_limited({1, 32});
    REQUIRE(bytes_limited.beginFrame(input(1)));
    CHECK_FALSE(bytes_limited.stage(opaque, nullptr, &error));
    CHECK_FALSE(bytes_limited.pending());
    History invalid_palette;
    batches[0].skin_joint_count = 15;
    REQUIRE(invalid_palette.beginFrame(input(1)));
    CHECK_FALSE(invalid_palette.stage(opaque, nullptr, &error));
    CHECK_FALSE(invalid_palette.pending());
}

TEST_CASE("Aurora camera bridge preserves viewer pixel reprojection without remapping reverse depth", "[temporal-history][aurora]")
{
    const std::array<float,16> projection{.5f,0,0,0, 0,1.3f,0,0, 0,0,0,-1, 0,0,.1f,0};
    std::array<float,16> reprojection{}; identity(reprojection.data());
    reprojection[12]=.2f; reprojection[13]=.3f;
    ALMetalAuroraFrame frame;
    alMetalAuroraSetViewerCamera(frame,projection.data(),reprojection.data());
    const auto mul=[](const auto& m,const std::array<float,4>& p)
    {
        std::array<float,4> result{};
        for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c) result[r]+=m[c*4+r]*p[c];
        return result;
    };
    const std::array<float,4> point{.4f,.25f,-2.f,1.f};
    const auto viewer=mul(projection,point), aurora=mul(frame.projection,point);
    CHECK((viewer[0]/viewer[3]*.5f+.5f)==Approx(aurora[0]/aurora[3]*.5f+.5f));
    CHECK((viewer[1]/viewer[3]*.5f+.5f)==Approx(.5f-aurora[1]/aurora[3]*.5f));
    CHECK(aurora[2]/aurora[3]==Approx(.05f));
    const std::array<float,4> current{.1f,-.2f,.05f,1.f};
    const auto previous=mul(frame.motion_reprojection,current);
    CHECK((previous[0]-current[0])*.5f==Approx(.1f));
    CHECK(-(previous[1]-current[1])*.5f==Approx(.15f));
    CHECK(previous[2]==current[2]);
}
