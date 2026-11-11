/**
 * @file alrender2dutils_metal_test.cpp
 * @brief Original 2D helper invariants through native buffers and UI pixels.
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

#include "llfontmetal_fixture.h"

#include "../llgltexture.h"
#include "../llrender2dutils.h"
#include "../llvertexbuffer.h"
#include "llimage.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <functional>
#include <list>
#include <vector>

namespace
{
    struct llrender2dutils_data : ll_test::NativeFontUI
    {
        bool ui_matrix = false;
        llrender2dutils_data()
        {
            initRenderer();
            gGL.pushUIMatrix();
            gGL.loadUIIdentity();
            ui_matrix = true;
        }
        ~llrender2dutils_data()
        {
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            if (ui_matrix) gGL.popUIMatrix();
        }

        static LLPointer<LLGLTexture> makeTexture(U16 width, U16 height)
        {
            LLPointer<LLImageRaw> raw = new LLImageRaw(width, height, 4);
            std::memset(raw->getData(), 0xFF, static_cast<size_t>(width) * height * 4);
            LLPointer<LLGLTexture> texture = new LLGLTexture(/*usemipmaps=*/false);
            REQUIRE(texture->createMetalTexture(0, raw));
            REQUIRE(texture->getGLTexture()->getMetalTexture());
            CHECK_FALSE(texture->hasGLTexture());
            return texture;
        }

        struct Bounds
        {
            F32 min_x = 0.f;
            F32 max_x = 0.f;
            F32 min_y = 0.f;
            F32 max_y = 0.f;
            size_t count = 0;
        };

        // Read the actual published native allocation rather than the CPU
        // producer's arrays, retaining the original upload oracle.
        template <typename Draw>
        static Bounds boundsOf(Draw draw)
        {
            std::list<LLVertexBufferData> capture;
            gGL.beginList(&capture);
            draw();
            gGL.flush();
            gGL.endList();

            Bounds b;
            const U32 stride = LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX];
            for (LLVertexBufferData& data : capture)
            {
                if (data.mVB.isNull() || data.mCount == 0)
                {
                    continue;
                }
                data.mVB->setBuffer();
                std::vector<F32> floats(static_cast<size_t>(data.mCount) * stride / sizeof(F32));
                REQUIRE(data.mVB->hasPublishedMetalVertexBuffer());
                const auto* buffer = data.mVB->getMetalVertexBuffer();
                REQUIRE(buffer);
                std::string error;
                REQUIRE(buffer->read(data.mVB->getOffset(LLVertexBuffer::TYPE_VERTEX),
                                     floats.data(), data.mCount * stride, &error));
                INFO(error);
                for (U32 i = 0; i < data.mCount; ++i)
                {
                    const F32* v = floats.data() + static_cast<size_t>(i) * stride / sizeof(F32);
                    if (b.count == 0)
                    {
                        b.min_x = b.max_x = v[0];
                        b.min_y = b.max_y = v[1];
                    }
                    b.min_x = llmin(b.min_x, v[0]);
                    b.max_x = llmax(b.max_x, v[0]);
                    b.min_y = llmin(b.min_y, v[1]);
                    b.max_y = llmax(b.max_y, v[1]);
                    ++b.count;
                }
            }
            LLVertexBuffer::unbind();
            return b;
        }

        static void require_near(const std::string& what, F32 actual, F32 expected)
        {
            INFO(what);
            CAPTURE(actual, expected);
            REQUIRE(fabsf(actual - expected) < 1e-3f);
        }
    };


    // A rotated image turns about its own centre and keeps its size. An
    // odd-sized image at a quarter turn covers the same pixels it does
    // unrotated -- an open folder arrow sits where the closed one did.
    TEST_CASE_METHOD(llrender2dutils_data, "native 2D helpers: a rotated image turns about its own centre and keeps its size", "[native-2d-utils]")
    {
        LLPointer<LLGLTexture> texture = makeTexture(4, 4);

        const Bounds flat = boundsOf([&]() {
            gl_draw_scaled_rotated_image(10, 20, 9, 9, 0.f, texture.get());
        });
        UNSCOPED_INFO("unrotated, one quad");
        REQUIRE(flat.count == (size_t)6);
        require_near("unrotated left", flat.min_x, 10.f);
        require_near("unrotated right", flat.max_x, 19.f);
        require_near("unrotated bottom", flat.min_y, 20.f);
        require_near("unrotated top", flat.max_y, 29.f);

        const Bounds turned = boundsOf([&]() {
            gl_draw_scaled_rotated_image(10, 20, 9, 9, 90.f, texture.get());
        });
        UNSCOPED_INFO("turned, one quad");
        REQUIRE(turned.count == (size_t)6);
        require_near("turned left", turned.min_x, flat.min_x);
        require_near("turned right", turned.max_x, flat.max_x);
        require_near("turned bottom", turned.min_y, flat.min_y);
        require_near("turned top", turned.max_y, flat.max_y);
    }

    // The turn is a turn: at a quarter turn a wide image stands tall, and
    // it does so about the centre of the rect it was asked to fill.
    TEST_CASE_METHOD(llrender2dutils_data, "native 2D helpers: a quarter turn stands a wide image tall about the rect's centre", "[native-2d-utils]")
    {
        LLPointer<LLGLTexture> texture = makeTexture(4, 4);

        const Bounds turned = boundsOf([&]() {
            gl_draw_scaled_rotated_image(10, 20, 8, 2, 90.f, texture.get());
        });
        UNSCOPED_INFO("one quad");
        REQUIRE(turned.count == (size_t)6);
        // Centre (14, 21); half extents swap to (1, 4).
        require_near("left", turned.min_x, 13.f);
        require_near("right", turned.max_x, 15.f);
        require_near("bottom", turned.min_y, 17.f);
        require_near("top", turned.max_y, 25.f);
    }

    // Rectangles added to one batch are one draw, where gl_rect_2d makes
    // a draw of each; and they are the same rectangles.
    TEST_CASE_METHOD(llrender2dutils_data, "native 2D helpers: gl_rect_2d_in_batch draws the same rectangles as gl_rect_2d in one draw", "[native-2d-utils]")
    {
        const auto count_draws = [](const std::function<void()>& draw) {
            std::list<LLVertexBufferData> capture;
            gGL.beginList(&capture);
            draw();
            gGL.flush();
            gGL.endList();
            size_t vertices = 0;
            for (const LLVertexBufferData& data : capture)
            {
                vertices += data.mCount;
            }
            return std::make_pair(capture.size(), vertices);
        };
        const auto apiece = count_draws([] {
            for (S32 i = 0; i < 40; ++i)
            {
                gl_rect_2d(i * 5, 30, i * 5 + 4, 20, LLColor4::white);
            }
        });
        const auto batched = count_draws([] {
            gGL.getTextureSlot(0)->unbind();
            gGL.begin(LLRender::TRIANGLES);
            for (S32 i = 0; i < 40; ++i)
            {
                gl_rect_2d_in_batch(i * 5, 30, i * 5 + 4, 20, LLColor4::white);
            }
            gGL.end();
        });
        UNSCOPED_INFO("a draw apiece");
        CHECK(apiece.first == (size_t)40);
        UNSCOPED_INFO("one draw in a batch");
        CHECK(batched.first == (size_t)1);
        UNSCOPED_INFO("the same vertices");
        CHECK(batched.second == apiece.second);

        const Bounds b = boundsOf([] {
            gGL.getTextureSlot(0)->unbind();
            gGL.begin(LLRender::TRIANGLES);
            gl_rect_2d_in_batch(10, 30, 20, 25, LLColor4::white);
            gl_rect_2d_in_batch(40, 60, 50, 55, LLColor4::white);
            gGL.end();
        });
        require_near("left", b.min_x, 10.f);
        require_near("right", b.max_x, 50.f);
        require_near("bottom", b.min_y, 25.f);
        require_near("top", b.max_y, 60.f);
    }

    // A rect's outline covers the pixels just inside its edges, and
    // nothing else: at the window's corner too, where a line along the
    // edge of a pixel landing outside the rect is not drawn at all; and
    // with the UI scaled up, a screen pixel thick, not a unit.
    TEST_CASE_METHOD(llrender2dutils_data, "native 2D helpers: a rect's outline lights only the pixels just inside its edges", "[native-2d-utils]")
    {
        constexpr S32 W = WIDTH;
        constexpr S32 H = HEIGHT;
        const auto drawn = [&](const LLRect& rect, F32 scale) {
            REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
                &gUIProgram, nullptr, nullptr, nullptr,
                LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
            gGL.pushUIMatrix();
            gGL.scaleUI(scale, scale, 1.f);
            gl_rect_2d(rect, LLColor4::white, false);
            gGL.flush();
            gGL.popUIMatrix();
            std::string error;
            LLRender::EMetalUICaptureEndState state{};
            auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
            INFO(error);
            REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
            REQUIRE(output);
            std::vector<U8> pixels(W * H * 4);
            REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), W * 4, &error));
            INFO(error);
            return pixels;
        };
        for (F32 scale : { 1.f, 2.f })
        {
            for (const LLRect& rect : { LLRect(0, 10, 20, 0), LLRect(30, 40, 50, 20) })
            {
                const std::vector<U8> rgba = drawn(rect, scale);
                // The rect on the screen, in its pixels.
                const S32 left = static_cast<S32>(rect.mLeft * scale), right = static_cast<S32>(rect.mRight * scale);
                const S32 bottom = static_cast<S32>(rect.mBottom * scale), top = static_cast<S32>(rect.mTop * scale);
                for (S32 y = 0; y < 120; ++y)
                {
                    for (S32 x = 0; x < 120; ++x)
                    {
                        const bool inside = x >= left && x < right && y >= bottom && y < top;
                        const bool edge   = inside && (x == left || x == right - 1 || y == bottom || y == top - 1);
                        const U8   red    = rgba[(static_cast<size_t>(y) * W + x) * 4];
                        if ((red == 255) != edge)
                        {
                            CAPTURE(rect.mLeft, rect.mBottom, scale, x, y, edge, static_cast<int>(red));
                            FAIL("a pixel off the outline is lit or one on it is not");
                        }
                    }
                }
            }
        }
    }
}
