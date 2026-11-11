/**
 * @file aluimetal_fixture.h
 * @brief One window-free native device/font/UI session for llui pixel tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#ifndef AL_ALUIMETAL_FIXTURE_H
#define AL_ALUIMETAL_FIXTURE_H

#include "../../llrender/tests/llfontmetal_fixture.h"
#include "../../llrender/altextureslot.h"
#include "alheadlessui_fixture.h"
#include "../llfocusmgr.h"
#include "../llui.h"
#include "llmortician.h"
#include <set>

namespace ll_test
{
    // Default-font getters and UI param defaults cache registry pointers. Keep
    // one session for the entire binary, separate from CPU-only UI tests.
    struct NativeUISession : NativeFontUI
    {
        std::unique_ptr<ALMetalTexture> black;
        LLFloaterView* previous_floater = gFloaterView;
        F32 previous_horizontal_dpi = LLFontGL::sHorizDPI;
        F32 previous_vertical_dpi = LLFontGL::sVertDPI;
        F32 previous_x_scale = LLFontGL::sScaleX;
        F32 previous_y_scale = LLFontGL::sScaleY;
        std::string previous_app_dir = LLFontGL::sAppDir;
        LLRect previous_dirty_rect = LLView::sDirtyRect;
        bool previous_dirty = LLView::sIsRectDirty;
        LLColor4 previous_shadow = LLFontGL::sShadowColor;
        S32 previous_viewport[4];
        const std::set<LLImageGL*> previous_images{
            LLImageGL::sImageList.begin(), LLImageGL::sImageList.end()};
        bool owns_ui = false;

        NativeUISession()
        {
            std::copy_n(gGLViewport, 4, previous_viewport);
            try
            {
                REQUIRE_FALSE(LLUI::instanceExists());
                initRenderer();
                // Clean worktrees need the build's staged font assets. Falling
                // back to system fonts changes the original placement oracle.
                REQUIRE(LLFile::isdir(std::string(LLUI_TEST_APP_DIR) + "/fonts"));
                // initClass is idempotent for the already-live font manager;
                // this first registry creates native textures from its first face.
                const bool ui_ok = HeadlessUI::get(false, true).ok();
                owns_ui = LLUI::instanceExists();
                REQUIRE(ui_ok);
                ALMetalTextureDescriptor desc;
                desc.width = WIDTH; desc.height = HEIGHT;
                desc.label = "native llui test opaque black backing";
                std::string error;
                black = ALMetalTexture::create(*device, desc, &error);
                INFO(error); REQUIRE(black);
                std::vector<U8> pixels(static_cast<size_t>(WIDTH) * HEIGHT * 4, 0);
                for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
                auto command = device->makeCommandBuffer("llui test backing", &error);
                REQUIRE(command);
                REQUIRE(black->upload(*command, 0, 0, {0, 0, WIDTH, HEIGHT},
                                      pixels.data(), WIDTH * 4, &error));
                REQUIRE(command->commitAndWait(&error));
            }
            catch (...)
            {
                std::string error;
                close(&error);
                throw;
            }
        }

        ~NativeUISession()
        {
            // Also unwind a failed setup/teardown while the base device lives.
            if (initialized)
            {
                std::string error;
                close(&error);
            }
        }

        void clearFramebuffer()
        {
            if (gGL.hasMetalUITextureCapture())
            {
                gGL.flush();
                gGL.endMetalUITextureCapture(false);
            }
            REQUIRE(gGL.beginMetalUITextureCapture(
                ALMetalPixelFormat::rgba8_unorm, &gUIProgram, black.get(),
                nullptr, nullptr, LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
        }

        // The displaced glFinish completed queued drawing. Ordered native
        // readback submits that exact frame and waits for its GPU completion;
        // the renderer retains it as the next segment's unchanged background.
        void finishFrame()
        {
            U8 pixel[4];
            REQUIRE(gGL.readMetalUITextureCaptureRGBA(0, 0, 1, 1, pixel, 4));
        }

        std::vector<U8> readFramebufferRGBA(S32 width, S32 height)
        {
            REQUIRE(width == WIDTH); REQUIRE(height == HEIGHT);
            std::vector<U8> pixels(static_cast<size_t>(width) * height * 4);
            REQUIRE(gGL.readMetalUITextureCaptureRGBA(
                0, 0, width, height, pixels.data(), width * 4));
            return pixels;
        }

        void finishCase()
        {
            if (!gGL.hasMetalUITextureCapture()) return;
            gGL.flush();
            std::string error;
            LLRender::EMetalUICaptureEndState state;
            auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
            INFO(error);
            CHECK((state == LLRender::EMetalUICaptureEndState::COMPLETE ||
                   state == LLRender::EMetalUICaptureEndState::EMPTY));
            if (state == LLRender::EMetalUICaptureEndState::COMPLETE) CHECK(output);
            // As in the original shared-context fixture, registry atlas pages
            // may grow between cases. Terminal teardown still requires release.
            StateGuard::accept("GL images outliving their context");
        }

        // Called before deleting this session, while HeadlessUI's settings and
        // directory still live. Views release cached text before their fonts;
        // all GPU owners retire before renderer/device teardown.
        bool close(std::string* error)
        {
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            gFocusMgr.setMouseCapture(nullptr);
            gFocusMgr.setKeyboardFocus(nullptr);
            gFocusMgr.setTopCtrl(nullptr);
            LLMortician::updateClass();
            if (owns_ui)
            {
                auto* ui = LLUI::getInstance();
                LLView* root = ui->getRootView();
                ui->setRootView(nullptr);
                delete root;
                delete gFloaterView;
                gFloaterView = previous_floater;
                LLUI::deleteSingleton();
                owns_ui = false;
                LLFontGL::destroyDefaultFonts();
            }
            ALFontShaping::clearCache();
            LLFontManager::cleanupClass();

            // ALTextView::squiggleTexture (original and native) owns one local
            // static LLPointer, outside the font registry. This is the sole
            // non-font LLImageGL producer in these three suites. Identify that
            // exact remaining owner by its unique original wave dimensions,
            // RGBA/no-mip storage and sole static reference. Retire its native
            // backing, but leave the intentional process-static metadata alive.
            LLImageGL* squiggle = nullptr;
            bool images_ok = true;
            for (LLImageGL* image : LLImageGL::sImageList)
            {
                if (previous_images.contains(image)) continue;
                const S32 scale = image->getWidth() / 6;
                if (squiggle || scale < 1 || scale > 4 ||
                    image->getWidth() != 6 * scale ||
                    image->getHeight() != 8 * scale ||
                    image->getComponents() != 4 || image->getUseMipMaps() ||
                    image->getNumRefs() != 1)
                {
                    images_ok = false;
                    if (error)
                        *error += llformat("; unexpected image %p (%dx%d, %d channels, mip %d, refs %d)",
                            static_cast<void*>(image), image->getWidth(), image->getHeight(),
                            image->getComponents(), image->getUseMipMaps(), image->getNumRefs());
                    continue;
                }
                squiggle = image;
            }
            if (images_ok && squiggle) squiggle->destroyGLTexture();
            black.reset();
            const bool idle = shutdownRenderer(error);
            LLFontGL::sHorizDPI = previous_horizontal_dpi;
            LLFontGL::sVertDPI = previous_vertical_dpi;
            LLFontGL::sScaleX = previous_x_scale;
            LLFontGL::sScaleY = previous_y_scale;
            LLFontGL::sAppDir = previous_app_dir;
            LLFontGL::sShadowColor = previous_shadow;
            LLView::sDirtyRect = previous_dirty_rect;
            LLView::sIsRectDirty = previous_dirty;
            std::copy_n(previous_viewport, 4, gGLViewport);
            if (!images_ok && error)
                *error += "; native llui left an owner other than the static squiggle";
            return idle && images_ok;
        }
    };

    inline NativeUISession& uiMetal()
    {
        static NativeUISession* session = []
        {
            const size_t images_before = LLImageGL::sImageList.size();
            const auto resources_before = alMetalResourceAllocationStats();
            auto* made = new NativeUISession;
            // Registered after HeadlessUI: LIFO closes native UI first, then
            // releases its still-live settings/directory owner.
            Teardown::add([made, images_before, resources_before]
            {
                std::unique_ptr<NativeUISession> owner(made);
                std::string error;
                const bool idle = made->close(&error);
                owner.reset();
                if (!idle) throw std::runtime_error("native llui wait failed: " + error);
                if (LLImageGL::sImageList.size() > images_before + 1)
                    throw std::runtime_error("native llui session leaked LLImageGL owners");
                const auto resources_after = alMetalResourceAllocationStats();
                if (resources_after.image_textures_live != resources_before.image_textures_live ||
                    resources_after.image_texture_bytes_live != resources_before.image_texture_bytes_live)
                    throw std::runtime_error("native llui session leaked image backing/accounting");
            });
            StateGuard::accept();
            return made;
        }();
        return *session;
    }

    struct NativeUICase
    {
        NativeUICase() { uiMetal().clearFramebuffer(); }
        ~NativeUICase() { uiMetal().finishCase(); }
    };

    inline void installNativeWhiteTexture()
    {
        // An untextured native UI batch uses the renderer's owned white texel.
        // No synthetic image/name is needed to paint a rectangle in its colour.
        gGL.getTextureSlot(0)->unbind();
    }

    inline std::vector<U8> readNativeUIFramebufferRGBA(S32 width, S32 height)
    {
        return uiMetal().readFramebufferRGBA(width, height);
    }
}
#endif
