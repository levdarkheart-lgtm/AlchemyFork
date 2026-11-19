/**
 * @file llmodel_test.cpp
 * @brief A model's generated normals and levels of detail against its skin
 *        weights.
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

#include "llmodel.h"
#include "almeshsimplifier.h"
#include "llmath.h"
#include "hbxxh.h"

#include <array>
#include <cstring>
#include <iostream>
#include <new>
#include <utility>

#include <catch2/catch_test_macros.hpp>

namespace
{
    LLVolumeParams meshParams()
    {
        LLVolumeParams params;
        params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
        return params;
    }

    // A latitude-longitude sphere of one face, its seam column twice, and a
    // skin weight at each of its positions, as a loader keys them.
    LLPointer<LLModel> riggedSphere(S32 columns, S32 rows)
    {
        LLPointer<LLModel> model = new LLModel(meshParams(), 0.f);
        model->setNumVolumeFaces(1);
        LLVolumeFace& face = model->getVolumeFace(0);
        const S32 vertices = (columns + 1) * (rows + 1);
        face.resizeVertices(vertices);
        for (S32 r = 0; r <= rows; ++r)
        {
            for (S32 c = 0; c <= columns; ++c)
            {
                const F32 theta = F_PI * r / rows;
                const F32 phi = F_TWO_PI * (c % columns) / columns;
                const F32 ring = (r == 0 || r == rows) ? 0.f : sinf(theta);
                const F32 height = r == 0 ? 1.f : (r == rows ? -1.f : cosf(theta));
                const S32 v = c + r * (columns + 1);
                face.mPositions[v].set(0.5f * ring * cosf(phi), 0.5f * ring * sinf(phi), 0.5f * height, 0.f);
                face.mNormals[v].set(ring * cosf(phi), ring * sinf(phi), height, 0.f);
                face.mTexCoords[v].set((F32)c / columns, 1.f - (F32)r / rows);
            }
        }
        std::vector<U16> indices;
        auto at = [&](S32 c, S32 r) { return (U16)(c + r * (columns + 1)); };
        for (S32 r = 0; r < rows; ++r)
        {
            for (S32 c = 0; c < columns; ++c)
            {
                if (r > 0)
                {
                    indices.insert(indices.end(), { at(c, r), at(c, r + 1), at(c + 1, r) });
                }
                if (r < rows - 1)
                {
                    indices.insert(indices.end(), { at(c + 1, r), at(c, r + 1), at(c + 1, r + 1) });
                }
            }
        }
        face.resizeIndices((S32)indices.size());
        std::copy(indices.begin(), indices.end(), face.mIndices);

        for (S32 v = 0; v < vertices; ++v)
        {
            const LLVector3 position(face.mPositions[v].getF32ptr());
            const F32 lean = 0.5f + position.mV[2];
            model->mSkinWeights[position] = { LLModel::JointWeight(0, 1.f - lean), LLModel::JointWeight(1, lean) };
        }
        return model;
    }

    // Every vertex finds its weights by its position as it is, without the
    // nearest-point search that a moved vertex would fall back on.
    bool weightsFound(LLModel& model)
    {
        for (S32 f = 0; f < model.getNumVolumeFaces(); ++f)
        {
            const LLVolumeFace& face = model.getVolumeFace(f);
            for (S32 v = 0; v < face.mNumVertices; ++v)
            {
                if (model.mSkinWeights.find(LLVector3(face.mPositions[v].getF32ptr())) == model.mSkinWeights.end())
                {
                    return false;
                }
            }
        }
        return true;
    }

    TEST_CASE("llmodel: generated normals and a simplified level of detail keep each vertex's skin weights", "[llmodel]")
    {
        LLPointer<LLModel> model = riggedSphere(32, 16);
        REQUIRE(weightsFound(*model));

        REQUIRE(model->generateNormals(30.f * DEG_TO_RAD));
        CHECK(model->getVolumeFace(0).mNumVertices > 0);
        CHECK(weightsFound(*model));

        using Seams = ALMeshSimplifier::Seams;
        for (Seams seams : { Seams::ALL, Seams::NONE, Seams::SLOPPY })
        {
            LLPointer<LLModel> lod = new LLModel(meshParams(), 0.f);
            lod->setNumVolumeFaces(1);
            ALMeshSimplifier::Target goal;
            goal.decimator = 9.f;
            REQUIRE(ALMeshSimplifier(*model).simplifyModel(*lod, goal, seams) > 0.f);
            lod->mSkinWeights = model->mSkinWeights;
            CHECK(weightsFound(*lod));
        }
    }

    TEST_CASE("llmodel: a later face's failed normals keep the entire model unchanged", "[llmodel]")
    {
        LLPointer<LLModel> model = riggedSphere(8, 4);
        model->setNumVolumeFaces(2);
        model->getVolumeFace(1) = model->getVolumeFace(0);
        LLVolumeFace& first = model->getVolumeFace(0);
        LLVolumeFace& second = model->getVolumeFace(1);
        for (S32 v = 0; v < first.mNumVertices; ++v)
        {
            first.mNormals[v].set(1.f, 0.f, 0.f, 0.f);
        }

        // The first face can be generated, but the second cannot name a
        // vertex outside its buffer. Neither may publish part of the model.
        LLVolumeFace generated(first);
        REQUIRE(generated.generateNormals(30.f * DEG_TO_RAD, 0.f));
        second.mIndices[0] = (U16)second.mNumVertices;
        const auto* first_positions = first.mPositions;
        const auto* second_positions = second.mPositions;
        const auto* first_normals = first.mNormals;
        const S32 first_vertices = first.mNumVertices;

        CHECK_FALSE(model->generateNormals(30.f * DEG_TO_RAD));
        CHECK(first.mPositions == first_positions);
        CHECK(second.mPositions == second_positions);
        CHECK(first.mNormals == first_normals);
        REQUIRE(first.mNumVertices == first_vertices);
        CHECK(second.mIndices[0] == second.mNumVertices);
        for (S32 v = 0; v < first.mNumVertices; ++v)
        {
            CHECK(first.mNormals[v][0] == 1.f);
            CHECK(first.mNormals[v][1] == 0.f);
            CHECK(first.mNormals[v][2] == 0.f);
        }
    }
}

namespace
{
    // Deterministically expose a constructor which leaves the trivial SIMD
    // bind-shape matrix untouched, without relying on allocator contents.
    class PoisonedSkin
    {
        alignas(LLMeshSkinInfo) std::array<unsigned char, sizeof(LLMeshSkinInfo)> mStorage;
    public:
        template<typename... Args> explicit PoisonedSkin(Args&&... args)
        {
            std::memset(mStorage.data(), 0xcd, mStorage.size());
            skin = new (mStorage.data()) LLMeshSkinInfo(std::forward<Args>(args)...);
        }
        ~PoisonedSkin() { skin->~LLMeshSkinInfo(); }
        PoisonedSkin(const PoisonedSkin&) = delete;
        PoisonedSkin& operator=(const PoisonedSkin&) = delete;
        LLMeshSkinInfo* skin;
    };

    void skinMatrix(LLSD& destination, const LLMatrix4a& matrix)
    {
        for (U32 row = 0; row < 4; ++row)
            for (U32 column = 0; column < 4; ++column)
                destination[row * 4 + column] = matrix.mMatrix[row][column];
    }

    LLSD authoredSkin(bool bind_shape)
    {
        LLSD data;
        data["joint_names"][0] = "mPelvis";
        data["joint_names"][1] = "mTorso";
        skinMatrix(data["inverse_bind_matrix"][0], LLMatrix4a::translation(1.25f, -2.5f, 3.75f));
        skinMatrix(data["inverse_bind_matrix"][1], LLMatrix4a::scaling(.5f, 2.f, 4.f));
        if (bind_shape)
            skinMatrix(data["bind_shape_matrix"], LLMatrix4a::scaling(2.f, 3.f, 4.f));
        return data;
    }

    void sameSkinMatrix(const LLMatrix4a& actual, const LLMatrix4a& expected)
    {
        CHECK(std::memcmp(actual.getF32ptr(), expected.getF32ptr(), 16 * sizeof(F32)) == 0);
    }

    TEST_CASE("PPD313: every skin constructor initializes an absent bind shape", "[llmodel][PPD313-init]")
    {
        PoisonedSkin empty;
        sameSkinMatrix(empty.skin->mBindShapeMatrix, LLMatrix4a::identity());
        CHECK(empty.skin->mHash == 0);
        LLSD data = authoredSkin(false);
        const LLUUID mesh_id("00112233-4455-6677-8899-aabbccddeeff");
        PoisonedSkin decoded(data), identified(mesh_id, data);
        for (const LLMeshSkinInfo* skin : {decoded.skin, identified.skin})
        {
            sameSkinMatrix(skin->mBindShapeMatrix, LLMatrix4a::identity());
            REQUIRE(skin->mInvBindMatrix.size() == 2);
            REQUIRE(skin->mBindPoseMatrix.size() == 2);
            sameSkinMatrix(skin->mBindPoseMatrix[0], skin->mInvBindMatrix[0]);
            sameSkinMatrix(skin->mBindPoseMatrix[1], skin->mInvBindMatrix[1]);
            CHECK(skin->mJointNums == std::vector<S32>{-1, -1});
        }
        CHECK(identified.skin->mMeshID == mesh_id);
        CHECK(decoded.skin->mHash == identified.skin->mHash);
    }

    TEST_CASE("PPD313: authored matrices and render batching hashes remain exact", "[llmodel][PPD313-valid]")
    {
        LLSD data = authoredSkin(true);
        PoisonedSkin decoded(data);
        sameSkinMatrix(decoded.skin->mBindShapeMatrix, LLMatrix4a::scaling(2.f, 3.f, 4.f));
        sameSkinMatrix(decoded.skin->mInvBindMatrix[0], LLMatrix4a::translation(1.25f, -2.5f, 3.75f));
        sameSkinMatrix(decoded.skin->mInvBindMatrix[1], LLMatrix4a::scaling(.5f, 2.f, 4.f));
        for (U32 joint = 0; joint < 2; ++joint)
        {
            LLMatrix4a expected;
            expected.setMul(decoded.skin->mBindShapeMatrix, decoded.skin->mInvBindMatrix[joint]);
            sameSkinMatrix(decoded.skin->mBindPoseMatrix[joint], expected);
        }
        const U64 original = decoded.skin->mHash;
        CHECK(original == 15304216647550519859ULL); // Captured from the unmodified 96f10353af production body.
        decoded.skin->updateHash();
        CHECK(decoded.skin->mHash == original);
        // Record the unmodified production hash before repair; the contained
        // red/green harness compares this same golden output across revisions.
        std::cout << "PPD313_VALID_HASH=" << original << '\n';
        data["inverse_bind_matrix"][0][12] = 1.25001;
        LLMeshSkinInfo sub_quantum(data);
        CHECK(sub_quantum.mHash == original);
        data["inverse_bind_matrix"][0][12] = 1.2502;
        LLMeshSkinInfo changed(data);
        CHECK(changed.mHash != original);
    }

    TEST_CASE("PPD313: empty and mismatched bindings have a defined empty hash", "[llmodel][PPD313-empty]")
    {
        LLMeshSkinInfo default_skin;
        default_skin.updateHash();
        HBXXH64 default_hash;
        CHECK(default_skin.mHash == default_hash.digest());
        for (U32 input = 0; input < 6; ++input)
        {
            CAPTURE(input);
            LLSD data;
            if (input == 1) data = LLSD::emptyMap();
            if (input == 2) data["joint_names"] = LLSD::emptyArray();
            if (input == 3) data["inverse_bind_matrix"] = LLSD::emptyArray();
            if (input == 4)
            {
                data["joint_names"] = LLSD::emptyArray();
                data["inverse_bind_matrix"] = LLSD::emptyArray();
            }
            if (input == 5)
            {
                data = authoredSkin(true);
                data["inverse_bind_matrix"].erase(1); // Existing mismatch policy drops bindings.
            }
            LLMeshSkinInfo decoded(data);
            CHECK(decoded.mJointNames.empty());
            CHECK(decoded.mJointNums.empty());
            CHECK(decoded.mInvBindMatrix.empty());
            CHECK(decoded.mBindPoseMatrix.empty());
            HBXXH64 empty_hash;
            CHECK(decoded.mHash == empty_hash.digest());
            const U64 hash = decoded.mHash;
            decoded.updateHash();
            CHECK(decoded.mHash == hash);
        }
    }

    TEST_CASE("PPD313: absent inverse data preserves existing joint admission", "[llmodel][PPD313-empty]")
    {
        LLSD data;
        data["joint_names"][0] = "mPelvis";
        LLMeshSkinInfo decoded(data);
        CHECK(decoded.mJointNames == std::vector<std::string>{"mPelvis"});
        CHECK(decoded.mJointNums == std::vector<S32>{-1});
        CHECK(decoded.mInvBindMatrix.empty());
        CHECK(decoded.mBindPoseMatrix.empty());
        HBXXH64 expected;
        expected.update(std::string("mPelvis"));
        const S32 unresolved = -1;
        expected.update(&unresolved, sizeof(unresolved));
        CHECK(decoded.mHash == expected.digest());
    }
}
