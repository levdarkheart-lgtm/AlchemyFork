/**
 * @file alfont_colrv1_failure_test.cpp
 * @brief Failed large COLRv1 paint cannot contaminate the next smaller glyph.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include <hb.h>
#include <hb-raster.h>
#include <catch2/catch_test_macros.hpp>

namespace
{
    thread_local bool failAfterPaint = false;

    hb_bool_t paintThenFail(hb_raster_paint_t* paint, hb_font_t* font, hb_codepoint_t glyph)
    {
        // Real HarfBuzz paint populates the real root/group surfaces. Return
        // an injected failure only after that work, before the wrapper renders
        // (and therefore before HarfBuzz's normal successful-path cleanup).
        const hb_bool_t painted = hb_raster_paint_glyph_or_fail(paint, font, glyph);
        if (failAfterPaint)
        {
            failAfterPaint = false;
            return false;
        }
        return painted;
    }
}

// Compile the production painter body with just its fallible library call
// intercepted. Other font tests exercise the ordinary linked painter; this
// copy can inject the otherwise nondeterministic mid-paint allocation failure
// without adding a production test hook or replacing hb-raster's surfaces.
#define ALFontColrV1Painter ALFontColrV1FailureTestPainter
#define hb_raster_paint_glyph_or_fail paintThenFail
#include "../alfontcolrv1.cpp"
#undef hb_raster_paint_glyph_or_fail
#undef ALFontColrV1Painter

#include "llfonttest_helpers.h"
#include <vector>

namespace
{
    TEST_CASE("COLRv1: a failed large paint leaves the next small glyph identical to a clean paint", "[ALFontColrV1][colrv1-failure]")
    {
        using namespace ll_test;
        const std::string path = std::string(kFontDir) + "Noto-COLRv1.ttf";
        if (!fileExists(path)) SKIP("Noto-COLRv1.ttf not present in test data dir");
        LLFontManager::initClass();
        struct Cleanup { ~Cleanup() { LLFontManager::cleanupClass(); } } cleanup;
        LLPointer<ALFontFace> small = gFontManagerp->getOrCreateFace(makeKey(path, 12.f));
        LLPointer<ALFontFace> large = gFontManagerp->getOrCreateFace(makeKey(path, 96.f));
        REQUIRE(small.notNull());
        REQUIRE(large.notNull());
        hb_codepoint_t glyph = 0;
        REQUIRE(hb_font_get_nominal_glyph(small->getHbFont(), 0x1F525, &glyph));
        REQUIRE(glyph != 0);
        ALFontColrV1FailureTestPainter painter;
        ALFontColrV1FailureTestPainter::Result clean, failed, recovered;
        const LLColor4U white(255, 255, 255, 255);
        REQUIRE(painter.paintGlyph(small->getHbFont(), glyph, 12.f, white, 0, clean));
        const S32 width = clean.mWidth, height = clean.mHeight, pitch = clean.mPitch;
        const S32 left = clean.mLeft, top = clean.mTop;
        const std::vector<U8> pixels(clean.mBitmap, clean.mBitmap + (size_t)height * pitch);
        failAfterPaint = true;
        CHECK_FALSE(painter.paintGlyph(large->getHbFont(), glyph, 96.f, white, 0, failed));
        CHECK_FALSE(failAfterPaint);
        CHECK(failed.mBitmap == nullptr);
        REQUIRE(painter.paintGlyph(small->getHbFont(), glyph, 12.f, white, 0, recovered));
        REQUIRE(recovered.mWidth == width);
        REQUIRE(recovered.mHeight == height);
        REQUIRE(recovered.mPitch == pitch);
        CHECK(recovered.mLeft == left);
        CHECK(recovered.mTop == top);
        CHECK(std::vector<U8>(recovered.mBitmap, recovered.mBitmap + (size_t)height * pitch) == pixels);
    }
}
