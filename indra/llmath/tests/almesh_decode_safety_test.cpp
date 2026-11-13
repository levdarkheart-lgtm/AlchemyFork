/**
 * @file almesh_decode_safety_test.cpp
 * @brief Untrusted mesh decode bounds and truncated skin-weight regressions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "llvolume.h"
#include "llsdserialize.h"
#include "v2math.h"
#include "v3math.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <vector>
#include <meshoptimizer.h>

namespace
{
    LLSD binary16(std::initializer_list<U16> values)
    {
        LLSD::Binary bytes;
        for (U16 value : values)
        {
            bytes.push_back((U8)value);
            bytes.push_back((U8)(value >> 8));
        }
        return LLSD(bytes);
    }

    LLSD wireTriangle()
    {
        LLSD face;
        face["Position"] = binary16({0, 0, 0, 65535, 0, 0, 0, 65535, 0});
        face["Normal"] = binary16({32768, 32768, 65535, 32768, 32768, 65535, 32768, 32768, 65535});
        face["TexCoord0"] = binary16({0, 0, 65535, 0, 0, 65535});
        face["TriangleList"] = binary16({0, 1, 2});
        face["PositionDomain"]["Min"] = LLVector3(-0.5f, -0.5f, -0.5f).getValue();
        face["PositionDomain"]["Max"] = LLVector3(0.5f, 0.5f, 0.5f).getValue();
        face["TexCoord0Domain"]["Min"] = LLVector2(0.f, 0.f).getValue();
        face["TexCoord0Domain"]["Max"] = LLVector2(1.f, 1.f).getValue();
        return face;
    }

    LLPointer<LLVolume> meshVolume()
    {
        LLVolumeParams params;
        params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
        params.setSculptID(LLUUID("00000000-0000-0000-0000-00000000000c"), LL_SCULPT_TYPE_MESH);
        return new LLVolume(params, 4.f);
    }

    bool decode(LLVolume& volume, LLSD faces)
    {
        // Exercise the real compressed-input API, including optimization.
        std::string packed = zip_llsd(faces);
        return volume.unpackVolumeFaces(reinterpret_cast<U8*>(packed.data()), (S32)packed.size());
    }

    LLSD oneFace(LLSD face)
    {
        LLSD faces = LLSD::emptyArray();
        faces.append(face);
        return faces;
    }

    TEST_CASE("Mesh decode: truncated influences and missing vertices have initialized weights", "[llvolume][mesh-decode-safety]")
    {
        struct Case { LLSD::Binary weights; S32 first_joint; };
        const Case cases[] = {
            {{}, 0}, {{5}, 0}, {{5, 0xff}, 0},
            {{5, 0xff, 0x7f}, 5}, {{5, 0xff, 0x7f, 0xff}, 5},
            {{5, 0xff, 0x7f, 7}, 5}, {{5, 0xff, 0x7f, 7, 0xff}, 5}
        };
        for (const auto& c : cases)
        {
            CAPTURE(c.weights.size());
            LLSD face = wireTriangle();
            face["Weights"] = LLSD(c.weights);
            LLPointer<LLVolume> volume = meshVolume();
            REQUIRE(decode(*volume, oneFace(face)));
            REQUIRE(volume->getNumVolumeFaces() == 1);
            const LLVolumeFace& decoded = volume->getVolumeFace(0);
            REQUIRE(decoded.mNumVertices == 3);
            REQUIRE(decoded.mWeights != nullptr);
            for (S32 i = 0; i < decoded.mNumVertices; ++i)
            {
                // Optimize may reorder vertices; identify the first wire
                // vertex by position rather than assuming its new index.
                const bool first = decoded.mPositions[i][0] < 0.f && decoded.mPositions[i][1] < 0.f;
                CHECK((S32)decoded.mWeights[i][0] == (first ? c.first_joint : 0));
                if (!first || c.first_joint == 0) CHECK(decoded.mWeights[i][0] == 0.999f);
                CHECK(decoded.mWeights[i][1] == 0.f);
                CHECK(decoded.mWeights[i][2] == 0.f);
                CHECK(decoded.mWeights[i][3] == 0.f);
            }
        }
    }

    TEST_CASE("Mesh decode: short normal and UV arrays use the absent attribute fallback", "[llvolume][mesh-decode-safety]")
    {
        // Keep the full streams as controls, and exercise each short stream
        // alone and together through the actual compressed decoder/optimizer.
        for (bool shortNormals : {false, true})
        {
            for (bool shortUVs : {false, true})
            {
                CAPTURE(shortNormals, shortUVs);
                LLSD face = wireTriangle();
                if (shortNormals) face["Normal"] = LLSD(LLSD::Binary{0x7f});
                if (shortUVs) face["TexCoord0"] = LLSD(LLSD::Binary{0x7f});
                LLPointer<LLVolume> volume = meshVolume();
                REQUIRE(decode(*volume, oneFace(face)));
                REQUIRE(volume->getNumVolumeFaces() == 1);
                const LLVolumeFace& decoded = volume->getVolumeFace(0);
                REQUIRE(decoded.mNumVertices == 3);
                for (S32 i = 0; i < decoded.mNumVertices; ++i)
                {
                    if (shortNormals)
                    {
                        CHECK(decoded.mNormals[i][0] == 0.f);
                        CHECK(decoded.mNormals[i][1] == 0.f);
                        CHECK(decoded.mNormals[i][2] == 0.f);
                    }
                    else
                    {
                        CHECK(decoded.mNormals[i][2] > 0.99f);
                    }
                    const F32 expectedU = !shortUVs && decoded.mPositions[i][0] > 0.f ? 1.f : 0.f;
                    const F32 expectedV = !shortUVs && decoded.mPositions[i][1] > 0.f ? 1.f : 0.f;
                    CHECK(decoded.mTexCoords[i][0] == expectedU);
                    CHECK(decoded.mTexCoords[i][1] == expectedV);
                }
            }
        }
    }

    TEST_CASE("Mesh decode: face and aggregate index budgets reject compressed oversized input", "[llvolume][mesh-decode-safety]")
    {
        const S32 most = LL_SCULPT_MESH_MAX_FACE_INDICES - LL_SCULPT_MESH_MAX_FACE_INDICES % 3;
        for (bool aggregate : {false, true})
        {
            CAPTURE(aggregate);
            LLSD faces = LLSD::emptyArray();
            for (S32 count : aggregate ? std::initializer_list<S32>{most, most, 6} : std::initializer_list<S32>{most + 3})
            {
                LLSD face = wireTriangle();
                face["TriangleList"] = LLSD(LLSD::Binary((size_t)count * sizeof(U16), 0));
                faces.append(face);
            }
            LLPointer<LLVolume> volume = meshVolume();
            REQUIRE(decode(*volume, oneFace(wireTriangle())));
            CHECK_FALSE(decode(*volume, faces));
            CHECK(volume->getNumVolumeFaces() == 0);
            // A refused LOD does not prevent a later valid one loading.
            CHECK(decode(*volume, oneFace(wireTriangle())));
        }
    }

    TEST_CASE("Mesh decode: empty geometry faces still count against the face budget", "[llvolume][mesh-decode-safety]")
    {
        for (S32 count : {LL_SCULPT_MESH_MAX_FACES, LL_SCULPT_MESH_MAX_FACES + 1})
        {
            LLSD faces = LLSD::emptyArray();
            for (S32 i = 0; i < count; ++i)
            {
                LLSD face;
                face["NoGeometry"] = true;
                faces.append(face);
            }
            LLPointer<LLVolume> volume = meshVolume();
            CHECK(decode(*volume, faces) == (count == LL_SCULPT_MESH_MAX_FACES));
            CHECK(volume->getNumVolumeFaces() == (count == LL_SCULPT_MESH_MAX_FACES ? count : 0));
        }
    }

    struct MeshAllocationFailures
    {
        static inline S32 remaining = -1;
        static inline S32 calls = 0;
        static inline S32 live = 0;
        static inline bool failed = false;

        static void* MESHOPTIMIZER_ALLOC_CALLCONV allocate(size_t bytes)
        {
            ++calls;
            if (remaining == 0)
            {
                remaining = -1;
                failed = true;
                throw std::bad_alloc();
            }
            if (remaining > 0) --remaining;
            void* result = ::operator new(bytes);
            ++live;
            return result;
        }
        static void MESHOPTIMIZER_ALLOC_CALLCONV deallocate(void* buffer)
        {
            if (buffer) --live;
            ::operator delete(buffer);
        }
        static void* MESHOPTIMIZER_ALLOC_CALLCONV defaultAllocate(size_t bytes) { return ::operator new(bytes); }
        static void MESHOPTIMIZER_ALLOC_CALLCONV defaultDeallocate(void* buffer) { ::operator delete(buffer); }
    };

    struct FailMeshAllocation
    {
        explicit FailMeshAllocation(S32 before_failure)
        {
            MeshAllocationFailures::remaining = before_failure;
            MeshAllocationFailures::calls = MeshAllocationFailures::live = 0;
            MeshAllocationFailures::failed = false;
            meshopt_setAllocator(MeshAllocationFailures::allocate, MeshAllocationFailures::deallocate);
        }
        ~FailMeshAllocation()
        {
            meshopt_setAllocator(MeshAllocationFailures::defaultAllocate, MeshAllocationFailures::defaultDeallocate);
        }
    };

    void cacheQuad(LLVolumeFace& face)
    {
        face.resizeVertices(4);
        face.resizeIndices(6);
        face.allocateWeights(4);
        face.mNormalizedScale.set(1.f, 1.f, 1.f);
        const U16 indices[] = {0, 1, 2, 2, 1, 3};
        std::copy(std::begin(indices), std::end(indices), face.mIndices);
        for (S32 i = 0; i < 4; ++i)
        {
            face.mPositions[i].set((F32)(i & 1), (F32)(i >> 1), 0.f, 0.f);
            face.mNormals[i].set(0.f, 0.f, 1.f, 0.f);
            face.mTexCoords[i].set((F32)(i & 1), (F32)(i >> 1));
            face.mWeights[i].set(0.999f, 0.f, 0.f, 0.f);
        }
    }

    TEST_CASE("Mesh cache: failed required weld allocation retains the original face", "[llvolume][mesh-allocation-safety]")
    {
        LLVolumeFace face;
        cacheQuad(face);
        const auto* positions = face.mPositions;
        const auto* indices = face.mIndices;
        const auto* weights = face.mWeights;
        const std::vector<U16> before(face.mIndices, face.mIndices + face.mNumIndices);
        bool optimized;
        {
            FailMeshAllocation fail(0);
            optimized = face.cacheOptimize(true);
        }
        CHECK(MeshAllocationFailures::failed);
        CHECK(MeshAllocationFailures::live == 0);
        CHECK_FALSE(optimized);
        CHECK(face.mOptimized);
        CHECK(face.mPositions == positions);
        CHECK(face.mIndices == indices);
        CHECK(face.mWeights == weights);
        CHECK(face.mTangents == nullptr);
        REQUIRE(face.mNumVertices == 4);
        REQUIRE(face.mNumIndices == 6);
        CHECK(std::equal(before.begin(), before.end(), face.mIndices));
        for (S32 i = 0; i < 4; ++i)
        {
            CHECK(face.mPositions[i][0] == (F32)(i & 1));
            CHECK(face.mPositions[i][1] == (F32)(i >> 1));
            CHECK(face.mWeights[i][0] == 0.999f);
        }
    }

    TEST_CASE("Mesh cache: each failed optional ordering allocation keeps valid triangles", "[llvolume][mesh-allocation-safety]")
    {
        S32 allocation_count;
        {
            LLVolumeFace control;
            cacheQuad(control);
            FailMeshAllocation count(-1);
            REQUIRE(control.cacheOptimize(false));
            allocation_count = MeshAllocationFailures::calls;
            REQUIRE(MeshAllocationFailures::live == 0);
        }
        REQUIRE(allocation_count > 0);
        for (S32 nth = 0; nth < allocation_count; ++nth)
        {
            CAPTURE(nth);
            LLVolumeFace face;
            cacheQuad(face);
            const auto* positions = face.mPositions;
            const auto* indices = face.mIndices;
            const auto* weights = face.mWeights;
            const std::vector<U16> before(face.mIndices, face.mIndices + face.mNumIndices);
            bool optimized;
            {
                FailMeshAllocation fail(nth);
                optimized = face.cacheOptimize(false);
            }
            CHECK(MeshAllocationFailures::failed);
            CHECK(MeshAllocationFailures::live == 0);
            CHECK(optimized);
            CHECK(face.mOptimized);
            CHECK(face.mPositions == positions);
            CHECK(face.mIndices == indices);
            CHECK(face.mWeights == weights);
            REQUIRE(face.mNumVertices == 4);
            REQUIRE(face.mNumIndices == 6);
            CHECK(std::equal(before.begin(), before.end(), face.mIndices));
        }
    }

    TEST_CASE("Mesh cache: optional ordering validates triangle indices", "[llvolume][mesh-cache-admission]")
    {
        SECTION("empty no-tangent work remains successful")
        {
            LLVolumeFace face;
            CHECK(face.cacheOptimize(false));
        }
        SECTION("invalid indices fail without tangent generation")
        {
            LLVolumeFace face;
            face.resizeVertices(3);
            face.resizeIndices(3);
            face.mIndices[0] = 0;
            face.mIndices[1] = 1;
            face.mIndices[2] = 3;
            CHECK_FALSE(face.cacheOptimize(false));
        }
        SECTION("incomplete triangles fail without tangent generation")
        {
            LLVolumeFace face;
            face.resizeVertices(3);
            face.resizeIndices(2);
            face.mIndices[0] = 0;
            face.mIndices[1] = 1;
            CHECK_FALSE(face.cacheOptimize(false));
        }
    }

    TEST_CASE("Mesh cache: high valence keeps the existing triangle order", "[llvolume][mesh-cache-budget]")
    {
        constexpr U32 rim = 10000;
        LLVolumeFace face;
        face.resizeVertices(rim + 1);
        face.resizeIndices(rim * 3);
        std::vector<U32> order(rim);
        std::iota(order.begin(), order.end(), 0u);
        std::shuffle(order.begin(), order.end(), std::mt19937(7));
        for (U32 i = 0; i < rim; ++i)
        {
            face.mIndices[i * 3] = 0;
            face.mIndices[i * 3 + 1] = (U16)(1 + order[i]);
            face.mIndices[i * 3 + 2] = (U16)(1 + (order[i] + 1) % rim);
        }
        const std::vector<U16> before(face.mIndices, face.mIndices + face.mNumIndices);
        REQUIRE(face.cacheOptimize(false));
        REQUIRE(face.mNumIndices == rim * 3);
        CHECK(std::equal(before.begin(), before.end(), face.mIndices));
    }
}
