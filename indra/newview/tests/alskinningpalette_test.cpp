/**
 * @file alskinningpalette_test.cpp
 * @brief Unit tests for finite rigged-mesh palette fallback.
 *
 * Copyright (c) 2026, Alchemy Viewer Project.
 *
 * The source code in this file is provided under the terms of the GNU Lesser
 * General Public License, version 2.1, without any warranty.
 */

#include "linden_common.h"

#include <catch2/catch_test_macros.hpp>

#include "../llskinningutil.h"

#include <array>
#include <cstring>
#include <limits>

TEST_CASE(
    "ALSkinningPalette: non-finite joints use the finite fallback",
    "[ALSkinningPalette]")
{
    const LLMatrix4a first = LLMatrix4a::translation(1.f, 2.f, 3.f);
    const LLMatrix4a fallback =
        LLMatrix4a::translation(40.f, 50.f, 60.f);
    const LLMatrix4a third = LLMatrix4a::scaling(2.f, 3.f, 4.f);
    std::array<LLMatrix4a, 3> palette{{first, fallback, third}};
    palette[1].mMatrix[2].set(
        0.f, std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f);

    CHECK(LLSkinningUtil::stabilizeMatrixPalette(
              palette.data(), palette.size(), fallback) == 1);
    CHECK(palette[0] == first);
    CHECK(palette[1] == fallback);
    CHECK(palette[2] == third);

    CHECK(LLSkinningUtil::stabilizeMatrixPalette(
              palette.data(), palette.size(), fallback) == 0);
    CHECK(LLSkinningUtil::stabilizeMatrixPalette(
              nullptr, palette.size(), fallback) == 0);
}

TEST_CASE(
    "ALSkinningPalette: unused homogeneous lanes cannot replace a valid deformation",
    "[ALSkinningPalette]")
{
    LLMatrix4a valid;
    valid.mMatrix[0].set(0.f, 2.f, 0.f, 0.f);
    valid.mMatrix[1].set(-3.f, 0.f, 0.f, 0.f);
    valid.mMatrix[2].set(0.f, 0.f, 0.5f, 0.f);
    valid.mMatrix[3].set(10.f, 20.f, 30.f, 1.f);
    const F32 nan = std::numeric_limits<F32>::quiet_NaN();
    const F32 inf = std::numeric_limits<F32>::infinity();
    LLMatrix4a unused_nonfinite = valid;
    unused_nonfinite.mMatrix[0].set(0.f, 2.f, 0.f, nan);
    unused_nonfinite.mMatrix[1].set(-3.f, 0.f, 0.f, inf);
    unused_nonfinite.mMatrix[2].set(0.f, 0.f, 0.5f, -inf);
    unused_nonfinite.mMatrix[3].set(10.f, 20.f, 30.f, nan);
    const LLMatrix4a original = unused_nonfinite;
    const LLMatrix4a fallback = LLMatrix4a::translation(100.f, 200.f, 300.f);
    REQUIRE(LLSkinningUtil::isFiniteSkinningMatrix(unused_nonfinite));
    CHECK(LLSkinningUtil::stabilizeMatrixPalette(&unused_nonfinite, 1, fallback) == 0);
    CHECK(std::memcmp(&unused_nonfinite, &original, sizeof(original)) == 0);

    // Execute the actual CPU position consumer, rather than only testing the
    // classifier: it must retain the authored rotation, scale and translation.
    const LLVector4a point(2.f, 3.f, 4.f, 1.f);
    LLVector4a expected, actual, substitute;
    valid.affineTransform(point, expected);
    unused_nonfinite.affineTransform(point, actual);
    fallback.affineTransform(point, substitute);
    REQUIRE(actual.isFinite3());
    const std::array<F32, 3> known{{1.f, 24.f, 32.f}};
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        CHECK(actual.getF32ptr()[axis] == known[axis]);
        CHECK(actual.getF32ptr()[axis] == expected.getF32ptr()[axis]);
        CHECK(actual.getF32ptr()[axis] != substitute.getF32ptr()[axis]);
    }
}

TEST_CASE(
    "ALSkinningPalette: every consumed affine coefficient rejects NaN and infinity",
    "[ALSkinningPalette]")
{
    const LLMatrix4a good = LLMatrix4a::translation(10.f, 20.f, 30.f);
    const LLMatrix4a fallback = LLMatrix4a::translation(40.f, 50.f, 60.f);
    for (U32 row = 0; row < 4; ++row)
    {
        for (U32 axis = 0; axis < 3; ++axis)
        {
            for (const F32 bad : {std::numeric_limits<F32>::quiet_NaN(),
                                  std::numeric_limits<F32>::infinity(),
                                  -std::numeric_limits<F32>::infinity()})
            {
                CAPTURE(row, axis);
                std::array<LLMatrix4a, 3> palette{{good, good, good}};
                std::array<F32, 4> values;
                std::memcpy(values.data(), good.mMatrix[row].getF32ptr(), sizeof(values));
                values[axis] = bad;
                palette[1].mMatrix[row].loadua(values.data());
                CHECK_FALSE(LLSkinningUtil::isFiniteSkinningMatrix(palette[1]));
                CHECK(LLSkinningUtil::stabilizeMatrixPalette(palette.data(), 3, fallback) == 1);
                CHECK(palette[0] == good);
                CHECK(palette[1] == fallback);
                CHECK(palette[2] == good);
            }
        }
    }
    LLMatrix4a large = good;
    large.mMatrix[0].set(1.e30f, -1.e30f, 1.e-30f, 0.f);
    const LLMatrix4a original = large;
    CHECK(LLSkinningUtil::stabilizeMatrixPalette(&large, 1, fallback) == 0);
    CHECK(std::memcmp(&large, &original, sizeof(original)) == 0);
}
