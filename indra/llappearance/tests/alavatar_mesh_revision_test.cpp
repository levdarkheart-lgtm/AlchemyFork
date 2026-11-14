/**
 * @file alavatar_mesh_revision_test.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llavatarjointmesh.h"
#include "../llpolymesh.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
class Mesh final : public LLAvatarJointMesh
{
public:
    U32 render(F32, bool, bool) override { return 0; }
};
}

TEST_CASE("Avatar mesh binding epoch detects same-pointer reload and null rebinding",
          "[avatar-mesh][native-pose]")
{
    LLPolyMeshSharedData data;
    LLPolyMesh polymesh(&data, nullptr);
    Mesh owner;
    LLAvatarJointMesh* base = &owner;
    const auto initial = owner.getMeshBindingRevision();
    base->setMesh(&polymesh);
    const auto bound = owner.getMeshBindingRevision();
    CHECK(bound != initial);
    CHECK(owner.getMesh() == &polymesh);
    // Same address, same viewer frame: setMesh frees/recreates skin binding
    // state even when no position/rotation/scale value changed.
    base->setMesh(&polymesh);
    CHECK(owner.getMeshBindingRevision() != bound);
    const auto reloaded = owner.getMeshBindingRevision();
    base->setMesh(nullptr);
    CHECK(owner.getMeshBindingRevision() != reloaded);
    CHECK(owner.getMesh() == nullptr);
}
