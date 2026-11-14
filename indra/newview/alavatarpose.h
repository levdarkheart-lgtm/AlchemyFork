/**
 * @file alavatarpose.h
 * @brief Pure view composition of immutable native avatar body poses.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once
#include "almetalavatarresources.h"
#include "llmath.h"
#include "llmatrix4a.h"
#include "m4math.h"
#include "m3math.h"
#include "v3math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace ALAvatarPose
{
// Original body-skin metadata order: a skin following a gap contributes its
// parent then child pivot; contiguous skins contribute only the child.
struct Pivots
{
    std::array<ALMetalAvatarPoseRecord::Pivot, ALMetalAvatarPoseRecord::max_joints> values{};
    std::size_t count = 0;
    bool previous_skin = false;
    void gap() noexcept { previous_skin = false; }
    void appendSkin(const ALMetalAvatarPoseRecord::Pivot& parent,
                    const ALMetalAvatarPoseRecord::Pivot& child) noexcept
    {
        if (count >= values.size()) return;
        if (!previous_skin)
        {
            values[count++] = parent;
            if (count == values.size()) return;
        }
        values[count++] = child;
        previous_skin = true;
    }
};

inline bool compose(
    const ALMetalAvatarPoseRecord& pose, const LLMatrix4a& model_view,
    F32* destination, std::size_t float_count) noexcept
{
    constexpr auto count = ALMetalAvatarPoseRecord::max_joints;
    constexpr auto floats = ALMetalAvatarPoseRecord::palette_float_count;
    if (!destination || float_count < floats) return false;
    for (std::size_t at = 0; at < 16; ++at)
        if (!std::isfinite(model_view.getF32ptr()[at])) return false;
    const auto& input = pose.inputs();
    std::array<LLMatrix4, count> joint_matrices{};
    std::array<LLMatrix3, count> joint_rotations{};
    for (std::size_t joint = 0; joint < input.joint_count; ++joint)
    {
        LLMatrix4a world, transformed;
        world.loadu(input.world_matrices[joint].data());
        transformed.setMulNoAlias(world, model_view);
        joint_matrices[joint] = transformed.toMatrix4();
        joint_rotations[joint] = joint_matrices[joint].getMat3();
    }
    for (std::size_t joint = 0; joint < input.pivot_count; ++joint)
    {
        const auto& value = input.pivots[joint];
        LLVector3 pivot(value[0], value[1], value[2]);
        pivot = pivot * joint_rotations[joint];
        joint_matrices[joint].translate(pivot);
    }
    std::array<F32, floats> output{};
    for (std::size_t joint = 0; joint < input.joint_count; ++joint)
    {
        joint_matrices[joint].transpose();
        for (std::size_t axis = 0; axis < 3; ++axis)
            std::memcpy(output.data() + (count * axis + joint) * 4,
                        joint_matrices[joint].mMatrix[axis], sizeof(F32) * 4);
    }
    for (F32 value : output)
        if (!std::isfinite(value)) return false;
    std::copy(output.begin(), output.end(), destination);
    return true;
}

}
