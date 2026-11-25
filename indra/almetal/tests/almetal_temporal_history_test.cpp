/**
 * @file almetal_temporal_history_test.cpp
 * @brief Main-world submission history and analytical camera/instance pose contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetaltemporalworldhistory.h"
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
