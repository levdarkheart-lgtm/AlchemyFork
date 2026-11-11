/**
 * @file alimageslot_gl_test.cpp
 * @brief ALImageSlot on the hidden window: what a unit issues to GL and what GL then holds, a redundant bind issuing
 *        nothing, refreshState putting back what was changed behind LLRender's back, forgetTextures dropping a
 *        deleted name, and the formats an image cannot take.
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

#include "../alimageslot.h"
#include "../llgl.h"
#include "../llglheaders.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llrendertarget.h"

#include "llheadlessgl_fixture.h"

#include "llimage.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>
#include <vector>

namespace
{
        constexpr U32 SIZE = 8;

        // What GL holds at image unit `unit`.
        struct UnitState
        {
            GLint mTexture = -1;
            GLint mLevel   = -1;
            GLint mLayered = -1;
            GLint mLayer   = -1;
            GLint mAccess  = -1;
            GLint mFormat  = -1;
        };

        UnitState unitState(U32 unit)
        {
            UnitState state;
            glGetIntegeri_v(GL_IMAGE_BINDING_NAME, unit, &state.mTexture);
            glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, unit, &state.mLevel);
            glGetIntegeri_v(GL_IMAGE_BINDING_LAYERED, unit, &state.mLayered);
            glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, unit, &state.mLayer);
            glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, unit, &state.mAccess);
            glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, unit, &state.mFormat);
            return state;
        }

    // The texture slots the tests change and compute's switch go back with mGL;
    // the image units are unbound here.
    struct alimageslot_data
    {
        ~alimageslot_data()
        {
            for (U32 unit = 0; unit < AL_NUM_IMAGE_SLOTS; ++unit)
            {
                gGL.getImageSlot(unit)->unbind();
            }
            mTarget.release();
            mOther.release();
        }

        // Compute in use, an R32F target with a mip level beyond the first and an RGBA16F one without, or the case
        // skipped where the context has no compute.
        void require_targets()
        {
            if (!ll_test::contextHasCompute())
            {
                SKIP("the context has no compute");
            }
            LLRender::sCompute = true;
            REQUIRE(mTarget.allocate(SIZE, SIZE, GL_R32F, false, false, ALTextureSlot::TT_TEXTURE, LLRenderTarget::MIPS_MANUAL));
            REQUIRE(mOther.allocate(SIZE, SIZE, GL_RGBA16F));
        }

        ll_test::SharedGLScope mGL;
        LLRenderTarget mTarget;
        LLRenderTarget mOther;
    };


    // A bind reaches GL as stated, a bind of what the unit already holds issues nothing, a change of any part of it
    // issues one call, and unbind returns the unit to texture 0 once.
    TEST_CASE_METHOD(alimageslot_data, "alimageslot_gl: a bind reaches GL as stated and only a change issues a call", "[alimageslot_gl]")
    {
        require_targets();
        ALImageSlot* slot = gGL.getImageSlot(2);
        CHECK(slot->getIndex() == 2);

        U32 binds = ALImageSlot::sImageBinds;
        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_WRITE, 1));
        UNSCOPED_INFO("the bind issues one call");
        CHECK(ALImageSlot::sImageBinds == binds + 1);
        UnitState state = unitState(2);
        CHECK(state.mTexture == (GLint)mTarget.getTexture());
        CHECK(state.mLevel == 1);
        CHECK(state.mLayered == GL_FALSE);
        CHECK(state.mAccess == GL_READ_WRITE);
        UNSCOPED_INFO("at the target's own format");
        CHECK(state.mFormat == GL_R32F);
        CHECK(slot->getCurrTexture() == mTarget.getTexture());

        binds = ALImageSlot::sImageBinds;
        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_WRITE, 1));
        UNSCOPED_INFO("the same bind issues nothing");
        CHECK(ALImageSlot::sImageBinds == binds);

        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_ONLY, 1));
        UNSCOPED_INFO("a new access issues one call");
        CHECK(ALImageSlot::sImageBinds == binds + 1);
        CHECK(unitState(2).mAccess == GL_READ_ONLY);

        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_ONLY, 0));
        UNSCOPED_INFO("a new level issues one call");
        CHECK(ALImageSlot::sImageBinds == binds + 2);
        CHECK(unitState(2).mLevel == 0);

        CHECK(slot->bind(&mOther, 0, ALImageSlot::READ_ONLY, 0));
        UNSCOPED_INFO("a new texture issues one call");
        CHECK(ALImageSlot::sImageBinds == binds + 3);
        state = unitState(2);
        CHECK(state.mTexture == (GLint)mOther.getTexture());
        UNSCOPED_INFO("at its own format");
        CHECK(state.mFormat == GL_RGBA16F);

        slot->unbind();
        UNSCOPED_INFO("unbind issues one call");
        CHECK(ALImageSlot::sImageBinds == binds + 4);
        CHECK(unitState(2).mTexture == 0);
        CHECK(slot->getCurrTexture() == 0u);
        slot->unbind();
        UNSCOPED_INFO("unbinding nothing issues nothing");
        CHECK(ALImageSlot::sImageBinds == binds + 4);

        CHECK(glGetError() == GLenum(GL_NO_ERROR));
    }

    // A unit changed behind LLRender's back is put back by refreshState, every part of it.
    TEST_CASE_METHOD(alimageslot_data, "alimageslot_gl: refreshState puts back every part of a unit changed behind its back", "[alimageslot_gl]")
    {
        require_targets();
        CHECK(gGL.getImageSlot(3)->bind(&mTarget, 0, ALImageSlot::WRITE_ONLY, 1));
        CHECK(gGL.getImageSlot(4)->bind(&mOther, 0, ALImageSlot::READ_ONLY));

        glBindImageTexture(3, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
        glBindImageTexture(4, mTarget.getTexture(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);
        UNSCOPED_INFO("clobbered");
        CHECK(unitState(3).mTexture == 0);

        gGL.refreshState();

        UnitState state = unitState(3);
        {
            INFO("unit 3 put back");
            CHECK(state.mTexture == (GLint)mTarget.getTexture());
            CHECK(state.mLevel == 1);
            CHECK(state.mAccess == GL_WRITE_ONLY);
            CHECK(state.mFormat == GL_R32F);
        }
        state = unitState(4);
        {
            INFO("unit 4 put back");
            CHECK(state.mTexture == (GLint)mOther.getTexture());
            CHECK(state.mAccess == GL_READ_ONLY);
            CHECK(state.mFormat == GL_RGBA16F);
        }

        CHECK(glGetError() == GLenum(GL_NO_ERROR));
    }

    // forgetTextures drops a unit holding a name about to be deleted, and only that one, so the name bound again
    // afterwards -- which GL is free to hand to a new texture -- is bound rather than skipped. And deleting a target
    // the viewer's way, through LLImageGL's deferred delete, reaches it.
    TEST_CASE_METHOD(alimageslot_data, "alimageslot_gl: forgetTextures drops only the units that held a deleted name", "[alimageslot_gl]")
    {
        require_targets();
        ALImageSlot* slot = gGL.getImageSlot(1);
        ALImageSlot* kept = gGL.getImageSlot(5);
        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_ONLY));
        CHECK(kept->bind(&mOther, 0, ALImageSlot::READ_ONLY));

        const U32 name = mTarget.getTexture();
        gGL.forgetTextures(1, &name);
        UNSCOPED_INFO("the slot forgot the name");
        CHECK(slot->getCurrTexture() == 0u);
        UNSCOPED_INFO("the other slot did not");
        CHECK(kept->getCurrTexture() == mOther.getTexture());

        const U32 binds = ALImageSlot::sImageBinds;
        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_ONLY));
        UNSCOPED_INFO("the forgotten name is bound, not skipped");
        CHECK(ALImageSlot::sImageBinds == binds + 1);

        // The viewer's way: the target's textures go on LLImageGL's free list and are deleted some frames on.
        const U32 other = mOther.getTexture();
        mOther.release();
        for (U32 frame = 0; frame < 8; ++frame)
        {
            LLImageGL::updateClass();
        }
        UNSCOPED_INFO("the deleted texture's slot forgot it");
        CHECK(kept->getCurrTexture() == 0u);
        UNSCOPED_INFO("GL dropped it too");
        CHECK(unitState(5).mTexture == 0);
        UNSCOPED_INFO("the live one stays");
        CHECK(slot->getCurrTexture() == name);
        CHECK(glIsTexture(other) == GL_FALSE);

        CHECK(glGetError() == GLenum(GL_NO_ERROR));
    }

    // What an image cannot be bound to: an sRGB texture -- which an LLImageGL made from an image is -- a depth
    // texture, and a unit past the slots. Each is refused, with nothing issued and the unit unchanged; the same image
    // with a linear format, and a volume, bind.
    TEST_CASE_METHOD(alimageslot_data, "alimageslot_gl: sRGB and depth textures and units past the slots are refused", "[alimageslot_gl]")
    {
        require_targets();
        ALImageSlot* slot = gGL.getImageSlot(6);
        CHECK(slot->bind(&mTarget, 0, ALImageSlot::READ_ONLY));
        const U32 binds = ALImageSlot::sImageBinds;

        LLPointer<LLImageRaw> raw = new LLImageRaw(4, 4, 4);
        std::memset(raw->getData(), 0x80, 4 * 4 * 4);
        LLPointer<LLImageGL> srgb = new LLImageGL(raw, false);
        CHECK(srgb->getStorageInternalFormat() == (S32)GL_SRGB8_ALPHA8);
        CHECK_FALSE(slot->bind(srgb.get(), ALImageSlot::READ_ONLY));

        LLRenderTarget depth;
        REQUIRE(depth.allocate(SIZE, SIZE, 0, true));
        CHECK_FALSE(slot->bindManual(depth.getDepth(), 0, false, 0, ALImageSlot::READ_ONLY,
                                                  GL_DEPTH_COMPONENT24));
        depth.release();

        CHECK_FALSE(gGL.getImageSlot(AL_NUM_IMAGE_SLOTS)->bind(&mTarget, 0, ALImageSlot::READ_ONLY));

        UNSCOPED_INFO("nothing issued");
        CHECK(ALImageSlot::sImageBinds == binds);
        UNSCOPED_INFO("the unit unchanged");
        CHECK(unitState(6).mTexture == (GLint)mTarget.getTexture());
        UNSCOPED_INFO("nor the slot");
        CHECK(slot->getCurrTexture() == mTarget.getTexture());

        LLPointer<LLImageGL> linear = new LLImageGL(4, 4, 4, false);
        linear->setExplicitFormat(GL_RGBA8, GL_RGBA);
        REQUIRE(linear->createGLTexture(0, raw.get()));
        CHECK(slot->bind(linear.get(), ALImageSlot::READ_ONLY));
        UnitState state = unitState(6);
        CHECK(state.mTexture == (GLint)linear->getTexName());
        CHECK(state.mFormat == GL_RGBA8);
        CHECK(state.mLayered == GL_FALSE);

        CHECK(glGetError() == GLenum(GL_NO_ERROR));
    }

}
