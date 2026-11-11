/**
 * @file aluiimage_metal_test.cpp
 * @brief LLUIImage's display-list cache holds an image's shape, not its place.
 *
 * An image is drawn at many positions -- every icon in a list, every
 * background of a floater being dragged -- and the recording made of it
 * has to serve all of them. These tests pin that a recording carries no
 * position of its own, and that moving the image replays it rather
 * than making another.
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
#include "../lluiimage.h"
#include "../llvertexbuffer.h"
#include "llimage.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <array>
#include <algorithm>
#include <initializer_list>
#include <vector>

namespace ll_test
{
    // Test-only view of LLUIImage's cache, friended in lluiimage.h.
    struct UIImageProbe
    {
        static size_t recordings(const LLUIImage& image)
        {
            return image.mDisplayLists.size();
        }

        static bool collectionEnabled() { return LLUIImage::sEnableDisplayListsCollection; }

        // Read the actual published native vertex allocation, preserving the
        // original buffer-readback invariant without a driver binding query.
        static std::vector<LLVector3> positions(const LLUIImage& image)
        {
            std::vector<LLVector3> out;
            const U32 stride = LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX];
            for (auto& entry : image.mDisplayLists)
            {
                for (LLVertexBufferData& data : entry.second.list)
                {
                    if (data.mVB.isNull() || data.mCount == 0)
                    {
                        continue;
                    }
                    data.mVB->setBuffer();
                    std::vector<F32> floats(static_cast<size_t>(data.mCount) * stride / sizeof(F32));
                    REQUIRE(data.mVB->hasPublishedMetalVertexBuffer());
                    const ALMetalBuffer* buffer = data.mVB->getMetalVertexBuffer();
                    REQUIRE(buffer);
                    std::string error;
                    REQUIRE(buffer->read(data.mVB->getOffset(LLVertexBuffer::TYPE_VERTEX),
                                         floats.data(), data.mCount * stride, &error));
                    INFO(error);
                    for (U32 i = 0; i < data.mCount; ++i)
                    {
                        const F32* v = floats.data() + static_cast<size_t>(i) * stride / sizeof(F32);
                        out.emplace_back(v[0], v[1], v[2]);
                    }
                }
            }
            LLVertexBuffer::unbind();
            return out;
        }
    };
}

namespace
{
    struct lluiimage_data : ll_test::NativeFontUI
    {
        bool previous_collection = ll_test::UIImageProbe::collectionEnabled();
        bool ui_matrix = false;
        lluiimage_data()
        {
            initRenderer();
            REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
                &gUIProgram, nullptr, nullptr, nullptr,
                LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
            gGL.pushUIMatrix(); gGL.loadUIIdentity(); ui_matrix = true;
            LLUIImage::enableDisplayListsCollection(true);
        }
        ~lluiimage_data()
        {
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            LLUIImage::cleanupClass();
            LLUIImage::enableDisplayListsCollection(previous_collection);
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
        struct Rectangle
        {
            S32 x, y, width, height;
            std::array<U8, 4> color{255,255,255,255};
        };
        void requirePixels(std::initializer_list<Rectangle> rectangles)
        {
            std::string error;
            LLRender::EMetalUICaptureEndState state{};
            auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
            INFO(error); REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
            REQUIRE(output);
            std::vector<U8> actual(WIDTH * HEIGHT * 4), expected(actual.size(), 0);
            REQUIRE(output->readback(*device, 0, 0, {}, actual.data(), WIDTH * 4, &error));
            for (const auto& r : rectangles)
                for (S32 y = r.y; y < r.y + r.height; ++y)
                    for (S32 x = r.x; x < r.x + r.width; ++x)
                        std::copy(r.color.begin(), r.color.end(),
                            expected.begin() + (static_cast<std::size_t>(y) * WIDTH + x) * 4);
            CHECK(actual == expected);
        }

        // A recording's bounds are the image's size, from the origin.
        static void require_extent(const std::vector<LLVector3>& verts, F32 width, F32 height)
        {
            REQUIRE_FALSE(verts.empty());
            F32 min_x = verts[0].mV[VX];
            F32 max_x = min_x;
            F32 min_y = verts[0].mV[VY];
            F32 max_y = min_y;
            for (const LLVector3& v : verts)
            {
                min_x = llmin(min_x, v.mV[VX]);
                max_x = llmax(max_x, v.mV[VX]);
                min_y = llmin(min_y, v.mV[VY]);
                max_y = llmax(max_y, v.mV[VY]);
            }
            REQUIRE(min_x == 0.f);
            REQUIRE(min_y == 0.f);
            REQUIRE(max_x == width);
            REQUIRE(max_y == height);
        }
    };


    // One recording serves every position the image is drawn at. A floater
    // being dragged is this test run at frame rate.
    TEST_CASE_METHOD(lluiimage_data, "LLUIImage: one recording serves every position but not every size or colour", "[LLUIImage][native-font-ui-admission]")
    {
        LLPointer<LLGLTexture> texture = makeTexture(4, 4);
        LLPointer<LLUIImage> image = new LLUIImage("probe", texture.get());

        image->draw(10, 10, 8, 8);
        UNSCOPED_INFO("the first draw records");
        CHECK(ll_test::UIImageProbe::recordings(*image) == (size_t)1);

        image->draw(100, 50, 8, 8);
        UNSCOPED_INFO("another position replays");
        CHECK(ll_test::UIImageProbe::recordings(*image) == (size_t)1);

        gGL.pushUIMatrix();
        gGL.translateUI(30.f, 40.f, 0.f);
        image->draw(10, 10, 8, 8);
        gGL.popUIMatrix();
        UNSCOPED_INFO("another UI offset replays");
        CHECK(ll_test::UIImageProbe::recordings(*image) == (size_t)1);

        image->draw(10, 10, 16, 8);
        UNSCOPED_INFO("another size is another recording");
        CHECK(ll_test::UIImageProbe::recordings(*image) == (size_t)2);

        image->draw(10, 10, 8, 8, LLColor4::red);
        UNSCOPED_INFO("another colour is another recording");
        CHECK(ll_test::UIImageProbe::recordings(*image) == (size_t)3);
        requirePixels({{10,10,16,8}, {100,50,8,8}, {40,50,8,8},
                       {10,10,8,8,{255,0,0,255}}});
    }

    // The recording holds the image's shape and nothing of where it was made:
    // drawn under a UI offset and away from the origin, its vertices still
    // span exactly [0, width] x [0, height].
    TEST_CASE_METHOD(lluiimage_data, "LLUIImage: a plain image's recording spans its size from the origin", "[LLUIImage][native-font-ui-admission]")
    {
        LLPointer<LLGLTexture> texture = makeTexture(4, 4);
        LLPointer<LLUIImage> image = new LLUIImage("probe", texture.get());

        gGL.pushUIMatrix();
        gGL.translateUI(30.f, 40.f, 0.f);
        image->draw(10, 10, 8, 6);
        gGL.popUIMatrix();

        const std::vector<LLVector3> verts = ll_test::UIImageProbe::positions(*image);
        UNSCOPED_INFO("a plain image is one quad");
        REQUIRE(verts.size() == (size_t)6);
        require_extent(verts, 8.f, 6.f);
        requirePixels({{40,50,8,6}});
    }

    // The nine-slice path works out its own placement from the UI offset, so
    // it is checked apart from the plain quad.
    TEST_CASE_METHOD(lluiimage_data, "LLUIImage: a nine-slice image's recording spans its size from the origin", "[LLUIImage][native-font-ui-admission]")
    {
        LLPointer<LLGLTexture> texture = makeTexture(16, 16);
        LLPointer<LLUIImage> image = new LLUIImage("probe", texture.get());
        image->setScaleRegion(LLRectf(0.25f, 0.75f, 0.75f, 0.25f));

        gGL.pushUIMatrix();
        gGL.translateUI(30.f, 40.f, 0.f);
        image->draw(10, 10, 32, 24);
        gGL.popUIMatrix();

        const std::vector<LLVector3> verts = ll_test::UIImageProbe::positions(*image);
        UNSCOPED_INFO("nine slices are eighteen triangles");
        REQUIRE(verts.size() == (size_t)54);
        require_extent(verts, 32.f, 24.f);
        requirePixels({{40,50,32,24}});
    }
}
