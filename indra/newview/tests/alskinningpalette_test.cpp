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
