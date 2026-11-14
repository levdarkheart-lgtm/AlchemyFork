/**
 * @file alavatarpose_test.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alavatarpose.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace
{
using Pose = ALMetalAvatarPoseRecord;
Pose::Matrix matrix(const LLMatrix4& source)
{
    Pose::Matrix result;
    std::memcpy(result.data(), source.mMatrix, sizeof(F32) * 16);
    return result;
}
LLVector3 apply(const std::array<F32, Pose::palette_float_count>& palette,
                std::size_t joint, const LLVector3& point)
{
    LLVector3 result;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        const auto* row = palette.data() + (Pose::max_joints * axis + joint) * 4;
        result.mV[axis] = row[0] * point.mV[0] + row[1] * point.mV[1] +
                         row[2] * point.mV[2] + row[3];
    }
    return result;
}
}

TEST_CASE("Native body pose retains exact 45 vec4 axis packing and pivot translation",
          "[avatar-pose][native-publication]")
{
    Pose::Inputs input;
    input.joint_count = input.pivot_count = 1;
    LLMatrix4 world;
    world.setTranslation(LLVector3(10.f, 20.f, 30.f));
    input.world_matrices[0] = matrix(world);
    input.pivots[0] = {1.f, 2.f, 3.f};
    const auto pose = Pose::create(input);
    REQUIRE(pose);
    const LLMatrix4a view = LLMatrix4a::translation(-5.f, 7.f, 9.f);
    std::array<F32, Pose::palette_float_count> packed;
    REQUIRE(ALAvatarPose::compose(*pose, view, packed.data(), packed.size()));
    const std::array<std::array<F32, 4>, 3> expected{{
        {1.f, 0.f, 0.f, 6.f}, {0.f, 1.f, 0.f, 29.f}, {0.f, 0.f, 1.f, 42.f}}};
    for (std::size_t axis = 0; axis < 3; ++axis)
        for (std::size_t joint = 0; joint < Pose::max_joints; ++joint)
            for (std::size_t lane = 0; lane < 4; ++lane)
                CHECK(packed[(Pose::max_joints * axis + joint) * 4 + lane] ==
                      (joint == 0 ? expected[axis][lane] : 0.f));
}

TEST_CASE("Native body skin pivot order retains parent gaps contiguous children and truncation",
          "[avatar-pose][native-publication]")
{
    ALAvatarPose::Pivots pivots;
    pivots.gap();
    pivots.appendSkin({1.f, 0.f, 0.f}, {2.f, 0.f, 0.f});
    pivots.appendSkin({3.f, 0.f, 0.f}, {4.f, 0.f, 0.f});
    pivots.gap();
    pivots.appendSkin({5.f, 0.f, 0.f}, {6.f, 0.f, 0.f});
    REQUIRE(pivots.count == 5);
    const std::array<F32, 5> expected{1.f, 2.f, 4.f, 5.f, 6.f};
    for (std::size_t at = 0; at < expected.size(); ++at)
        CHECK(pivots.values[at][0] == expected[at]);
    while (pivots.count < Pose::max_joints - 1)
        pivots.appendSkin({0.f, 0.f, 0.f}, {7.f, 0.f, 0.f});
    pivots.gap();
    pivots.appendSkin({8.f, 0.f, 0.f}, {9.f, 0.f, 0.f});
    REQUIRE(pivots.count == Pose::max_joints);
    CHECK(pivots.values.back()[0] == 8.f);
    const auto full = pivots.values;
    pivots.appendSkin({10.f, 0.f, 0.f}, {11.f, 0.f, 0.f});
    CHECK(pivots.count == Pose::max_joints);
    CHECK(pivots.values == full);
}

TEST_CASE("Native body world pose composes independently for rotated scaled main probe and shadow views",
          "[avatar-pose][native-publication]")
{
    Pose::Inputs input;
    input.joint_count = input.pivot_count = Pose::max_joints;
    std::array<LLMatrix4, Pose::max_joints> worlds;
    for (std::size_t joint = 0; joint < Pose::max_joints; ++joint)
    {
        const F32 j = static_cast<F32>(joint);
        worlds[joint].initAll(LLVector3(1.f + j * .02f, .8f, 1.2f),
            LLQuaternion(j * .031f, LLVector3::z_axis), LLVector3(j, -j, j * .25f));
        input.world_matrices[joint] = matrix(worlds[joint]);
        input.pivots[joint] = {.1f * j, -.2f, .3f};
    }
    const auto pose = Pose::create(input);
    REQUIRE(pose);
    std::array<F32, Pose::palette_float_count> first{};
    for (unsigned view_index = 0; view_index < 3; ++view_index)
    {
        LLMatrix4 scalar_view;
        scalar_view.initAll(LLVector3(1.f, 1.f, 1.f),
            LLQuaternion(.3f * view_index, LLVector3::y_axis),
            LLVector3(-10.f * view_index, 4.f, 7.f));
        LLMatrix4a view;
        view.loadu(&scalar_view.mMatrix[0][0]);
        std::array<F32, Pose::palette_float_count> packed;
        REQUIRE(ALAvatarPose::compose(*pose, view, packed.data(), packed.size()));
        if (view_index == 0) first = packed;
        else CHECK(packed != first);
        // Independent point-transform oracle checks matrix/pivot order, not
        // another copy of the palette packing implementation.
        const LLVector3 vertex(.75f, -.5f, 1.25f);
        for (std::size_t joint = 0; joint < Pose::max_joints; ++joint)
        {
            const auto& p = input.pivots[joint];
            const LLVector3 expected = ((vertex + LLVector3(p[0], p[1], p[2])) *
                                       worlds[joint]) * scalar_view;
            const auto actual = apply(packed, joint, vertex);
            for (unsigned axis = 0; axis < 3; ++axis)
                CHECK(actual.mV[axis] == Catch::Approx(expected.mV[axis]).margin(.0002f));
        }
    }
    CHECK(pose->inputs().world_matrices == input.world_matrices);
}

TEST_CASE("Native body pose publication copies metadata and survives replacement and producer destruction",
          "[avatar-pose][native-publication]")
{
    std::shared_ptr<const Pose> queued;
    {
        Pose::Inputs input;
        input.joint_count = 1;
        input.world_matrices[0] = matrix(LLMatrix4());
        input.frame = 77;
        input.pose_revision = 100;
        input.mesh_revision = 9;
        queued = Pose::create(input);
        REQUIRE(queued);
        input.world_matrices[0][12] = 123.f;
        ++input.pose_revision; // another owner mutation in the SAME frame
        ++input.mesh_revision; // same-pointer mesh/skin reload
        auto replacement = Pose::create(input);
        REQUIRE(replacement);
        CHECK(replacement->inputs().frame == queued->inputs().frame);
        CHECK(replacement->inputs().pose_revision != queued->inputs().pose_revision);
        CHECK(replacement->inputs().mesh_revision != queued->inputs().mesh_revision);
    }
    CHECK(queued->inputs().world_matrices[0][12] == 0.f);
    CHECK(queued->inputs().pose_revision == 100);
    std::array<F32, Pose::palette_float_count> output;
    CHECK(ALAvatarPose::compose(*queued, LLMatrix4a::identity(), output.data(), output.size()));
}

TEST_CASE("Native body pose rejects malformed inputs and preserves destination on composition failure",
          "[avatar-pose][native-publication]")
{
    Pose::Inputs input;
    input.joint_count = 16;
    CHECK_FALSE(Pose::create(input));
    input.joint_count = 1;
    input.pivot_count = 2;
    CHECK_FALSE(Pose::create(input));
    input.pivot_count = 1;
    input.world_matrices[0] = matrix(LLMatrix4());
    input.pivots[0][1] = std::numeric_limits<F32>::infinity();
    CHECK_FALSE(Pose::create(input));
    input.pivots[0] = {};
    input.world_matrices[0][3] = std::numeric_limits<F32>::quiet_NaN();
    CHECK_FALSE(Pose::create(input));
    input.world_matrices[0] = matrix(LLMatrix4());
    const auto pose = Pose::create(input);
    REQUIRE(pose);
    std::array<F32, Pose::palette_float_count> output;
    output.fill(-123.f);
    const auto untouched = output;
    CHECK_FALSE(ALAvatarPose::compose(*pose, LLMatrix4a::identity(), nullptr, output.size()));
    CHECK_FALSE(ALAvatarPose::compose(*pose, LLMatrix4a::identity(), output.data(), output.size() - 1));
    auto bad_view = LLMatrix4a::identity();
    bad_view.mMatrix[0].set(std::numeric_limits<F32>::infinity(), 0.f, 0.f, 0.f);
    CHECK_FALSE(ALAvatarPose::compose(*pose, bad_view, output.data(), output.size()));
    CHECK(output == untouched);
    const auto empty = Pose::create({});
    REQUIRE(empty);
    REQUIRE(ALAvatarPose::compose(*empty, LLMatrix4a::identity(), output.data(), output.size()));
    for (F32 value : output) CHECK(value == 0.f);
}
