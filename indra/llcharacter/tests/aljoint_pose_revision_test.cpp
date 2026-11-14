/**
 * @file aljoint_pose_revision_test.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../lljoint.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Joint pose epoch advances once at mutation and survives lazy world reads",
          "[LLJoint][native-pose]")
{
    LLJoint root, child, grandchild;
    root.addChild(&child);
    child.addChild(&grandchild);
    root.updateWorldMatrixChildren();
    const auto before = root.getPoseRevision();
    child.setPosition(LLVector3(2.f, 3.f, 4.f));
    CHECK(root.getPoseRevision() == before + 1);
    CHECK(child.getPoseRevision() == root.getPoseRevision());
    CHECK(grandchild.getPoseRevision() == root.getPoseRevision());
    const auto changed = root.getPoseRevision();
    (void)grandchild.getWorldMatrix();
    root.updateWorldMatrixChildren();
    (void)child.getWorldPosition();
    (void)child.getWorldRotation();
    CHECK(root.getPoseRevision() == changed);
    child.setPosition(LLVector3(2.f, 3.f, 4.f));
    CHECK(root.getPoseRevision() == changed);
    // Flags may already have been consumed by another view; the durable
    // epoch still distinguishes another mutation in the same viewer frame.
    child.setPosition(LLVector3(5.f, 3.f, 4.f));
    (void)grandchild.getWorldMatrix();
    CHECK(root.getPoseRevision() == changed + 1);
}

TEST_CASE("Joint relinks invalidate old and new roots without stale descendant root pointers",
          "[LLJoint][native-pose]")
{
    LLJoint first, second, child, grandchild;
    first.addChild(&child);
    child.addChild(&grandchild);
    const auto first_before = first.getPoseRevision();
    const auto second_before = second.getPoseRevision();
    second.addChild(&child);
    CHECK(first.getPoseRevision() != first_before);
    CHECK(second.getPoseRevision() != second_before);
    CHECK(child.getPoseRevision() == second.getPoseRevision());
    CHECK(grandchild.getPoseRevision() == second.getPoseRevision());
    const auto first_after = first.getPoseRevision();
    grandchild.setScale(LLVector3(2.f, 1.f, 1.f));
    CHECK(first.getPoseRevision() == first_after);
    CHECK(grandchild.getPoseRevision() == second.getPoseRevision());
    const auto second_after = second.getPoseRevision();
    second.removeAllChildren();
    CHECK(second.getPoseRevision() != second_after);
    CHECK(child.getRoot() == &child);
    CHECK(grandchild.getPoseRevision() == child.getPoseRevision());
    const auto detached = child.getPoseRevision();
    grandchild.setPosition(LLVector3(1.f, 2.f, 3.f));
    CHECK(child.getPoseRevision() != detached);
}

TEST_CASE("Joint pose epoch observes actual external parent movement without read-only churn",
          "[LLJoint][native-pose]")
{
    LLXformMatrix seat;
    seat.setPosition(LLVector3(10.f, 0.f, 0.f));
    seat.updateMatrix();
    LLJoint root, child;
    root.addChild(&child);
    root.getXform()->setParent(&seat);
    root.touch();
    root.updateWorldMatrixChildren();
    const auto before = root.getPoseRevision();
    root.touchIfXformParentMoved();
    CHECK(root.getPoseRevision() == before);
    seat.setPosition(LLVector3(20.f, 0.f, 0.f));
    seat.updateMatrix();
    root.touchIfXformParentMoved();
    CHECK(root.getPoseRevision() != before);
    const auto moved = root.getPoseRevision();
    (void)child.getWorldMatrix();
    root.touchIfXformParentMoved();
    CHECK(root.getPoseRevision() == moved);
}
