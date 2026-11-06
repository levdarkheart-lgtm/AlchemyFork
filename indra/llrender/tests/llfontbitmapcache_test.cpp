/**
 * @file llfontbitmapcache_test.cpp
 * @brief Unit tests for LLFontBitmapCache — atlas allocator, row/sheet
 *        packing, generation counter, sheet release/eviction.
 *
 * Pure-CPU tests cover bookkeeping. On Darwin a no-context Metal case also
 * proves exact atlas publication/readback and native release.
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

#include "../llfontbitmapcache.h"
#include "llimage.h"

#include <catch2/catch_test_macros.hpp>

#if LL_DARWIN
#include "almetaldevice.h"
#include "almetalresource.h"
#include <vector>
#endif

namespace
{
    // Construction takes a fresh value from sNextGeneration. Two
    // back-to-back instances must therefore land on distinct
    // generations — without per-instance uniqueness, vertex buffers
    // comparing their stored mLastFontCacheGen across an in-place
    // font reload would false-match (see llfontbitmapcache.h:139-147).
    TEST_CASE("LLFontBitmapCache: each new cache takes a fresh and higher generation", "[LLFontBitmapCache]")
    {
        LLFontBitmapCache a;
        LLFontBitmapCache b;
        CHECK(a.getCacheGeneration() != b.getCacheGeneration());
        CHECK(a.getCacheGeneration() > 0);
        CHECK(b.getCacheGeneration() > a.getCacheGeneration());
    }

    // Static getGlobalGeneration must reflect the last sNextGeneration
    // bump. Constructing a new cache bumps the counter, so the global
    // tick must equal the new instance's mGeneration immediately after.
    TEST_CASE("LLFontBitmapCache: getGlobalGeneration is the newest cache's generation", "[LLFontBitmapCache]")
    {
        const S32 before = LLFontBitmapCache::getGlobalGeneration();
        LLFontBitmapCache c;
        const S32 after  = LLFontBitmapCache::getGlobalGeneration();
        CHECK(after > before);
        CHECK(after == c.getCacheGeneration());
    }

    // init() seeds mBitmapWidth/Height; reset() clears them back to 0
    // and bumps the generation counter. Verify both ends.
    TEST_CASE("LLFontBitmapCache: init sizes the sheets; reset clears them and bumps the generation", "[LLFontBitmapCache]")
    {
        LLFontBitmapCache c;
        c.init(/*max_char_width=*/2, /*max_char_height=*/2);
        // 2*40=80 → next pow2 ≥ 80 → 128, capped at 1024.
        CHECK(c.getBitmapWidth() == 128);
        CHECK(c.getBitmapHeight() == 128);
        // Read after init, which resets and so bumps it too.
        const S32 gen_before_reset = c.getCacheGeneration();

        c.reset();
        CHECK(c.getBitmapWidth() == 0);
        CHECK(c.getBitmapHeight() == 0);
        CHECK(c.getCacheGeneration() > gen_before_reset);
    }

    // Out-of-range queries on a freshly-constructed (or reset) cache
    // must return safe defaults, not crash. EFontGlyphType::Count is
    // a sentinel — every accessor must reject it.
    TEST_CASE("LLFontBitmapCache: queries out of range or on an empty cache return safe defaults", "[LLFontBitmapCache]")
    {
        LLFontBitmapCache c;
        CHECK(c.getNumBitmaps(EFontGlyphType::Count) == 0u);
        CHECK(c.getImageRaw(EFontGlyphType::Count, 0) == nullptr);
        CHECK(c.getImageGL(EFontGlyphType::Count, 0) == nullptr);
        UNSCOPED_INFO("an empty Grayscale vector");
        CHECK(c.getImageRaw(EFontGlyphType::Grayscale, 0) == nullptr);
        CHECK(c.getSheetLastUsedTime(EFontGlyphType::Grayscale, 0) == 0.0);
        UNSCOPED_INFO("a slot that does not exist is not released");
        CHECK_FALSE(c.isSheetReleased(EFontGlyphType::Grayscale, 0));
    }

#if LL_DARWIN
    TEST_CASE("LLFontBitmapCache: glyph pages publish and release through Metal without GL", "[LLFontBitmapCache][metal-cutover]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        INFO(error);

        ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
        const bool previous_metal = LLFontBitmapCache::usesMetal();
        struct StateRestore
        {
            ALMetalDevice* device;
            bool metal;
            ~StateRestore()
            {
                LLFontBitmapCache::setTextureBackend(false, metal);
                LLImageGL::setMetalDevice(device);
            }
        } restore{previous_device, previous_metal};
        LLImageGL::setMetalDevice(device.get());
        LLFontBitmapCache::setTextureBackend(false, true);

        LLFontBitmapCache cache;
        cache.init(2, 2);
        S32 x = 0;
        S32 y = 0;
        U32 sheet = 0;
        REQUIRE(cache.nextOpenPos(3, 2, x, y,
                                  EFontGlyphType::Grayscale, sheet));

        LLImageRaw* raw = cache.getImageRaw(EFontGlyphType::Grayscale, sheet);
        LLImageGL* page = cache.getImageGL(EFontGlyphType::Grayscale, sheet);
        REQUIRE(raw);
        REQUIRE(page);
        CHECK_FALSE(page->getHasGLTexture());
        REQUIRE(page->getMetalTexture());

        for (S32 row = y; row < y + 2; ++row)
        {
            for (S32 column = x; column < x + 3; ++column)
            {
                const std::size_t offset =
                    (static_cast<std::size_t>(row) * raw->getWidth() + column) * 2;
                raw->getData()[offset] = 0x91;
                raw->getData()[offset + 1] = 0x62;
            }
        }
        REQUIRE(cache.publishGlyph(EFontGlyphType::Grayscale, sheet,
                                   x, y, 3, 2));

        const std::size_t byte_count =
            static_cast<std::size_t>(raw->getWidth()) * raw->getHeight() * 2;
        std::vector<U8> actual(byte_count);
        REQUIRE(page->getMetalTexture()->readback(
            *device, 0, 0, {}, actual.data(), raw->getWidth() * 2, &error));
        INFO(error);
        const std::size_t glyph_offset =
            (static_cast<std::size_t>(y) * raw->getWidth() + x) * 2;
        CHECK(actual[glyph_offset] == 0x91);
        CHECK(actual[glyph_offset + 1] == 0x62);

        cache.releaseSheet(EFontGlyphType::Grayscale, sheet);
        CHECK(cache.isSheetReleased(EFontGlyphType::Grayscale, sheet));
        CHECK(cache.getImageGL(EFontGlyphType::Grayscale, sheet) == nullptr);
    }
#endif
}
