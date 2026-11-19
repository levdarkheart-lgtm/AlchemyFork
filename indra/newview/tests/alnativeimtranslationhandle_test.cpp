/**
 * @file alnativeimtranslationhandle_test.cpp
 * @brief PPD-510: weak UI handles with an observer before the floater base.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../llvoiceclient.h"
#include "llfloater.h"
#include "llhandle.h"
#include "../../llui/tests/alheadlessui_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>

namespace
{
    // The actual observer and actual UI base, in the same order as the IM
    // floater. No voice client, session, renderer or network is constructed.
    class ObserverFloater final : public LLVoiceClientStatusObserver, public LLFloater
    {
    public:
        explicit ObserverFloater(const LLFloater::Params& params)
            : LLFloater(LLSD(), params)
        {
        }

        void onChange(EStatusType, const LLSD&, bool) override {}

        std::string message = "original draft";
    };

    struct NativeIMHandleFixture
    {
        ll_test::HeadlessUI& ui = ll_test::HeadlessUI::get();

        static std::unique_ptr<ObserverFloater> makeFloater()
        {
            LLFloater::Params params;
            params.name = "native_im_handle_probe";
            params.rect = LLRect(0, 100, 200, 0);
            return std::make_unique<ObserverFloater>(params);
        }
    };

    TEST_CASE_METHOD(NativeIMHandleFixture,
                     "NativeIMTranslation: a UI handle must adjust a preceding observer base",
                     "[NativeIMTranslationHandle][PPD-510]")
    {
        REQUIRE(ui.ok());
        auto floater = makeFloater();
        auto* ui_base = static_cast<LLFloater*>(floater.get());
        REQUIRE(static_cast<void*>(ui_base) != static_cast<void*>(floater.get()));

        // LLHandle really stores the UI-base address. Its typed-derived API
        // shares that tombstone and does not perform a C++ pointer adjustment.
        // Compare only: dereferencing this unsafe pointer would reproduce the
        // original crash, rather than be a valid test operation.
        const auto unsafe = floater->getDerivedHandle<ObserverFloater>();
        CHECK(static_cast<void*>(unsafe.get()) == static_cast<void*>(ui_base));
        CHECK(unsafe.get() != floater.get());

        const LLHandle<LLFloater> handle = floater->getHandle();
        auto* adjusted = static_cast<ObserverFloater*>(handle.get());
        REQUIRE(adjusted == floater.get());
        CHECK(adjusted->message == "original draft");
        adjusted->message = "translated result";
        CHECK(floater->message == "translated result");
    }

    TEST_CASE_METHOD(NativeIMHandleFixture,
                     "NativeIMTranslation: a captured UI handle expires before another chat is created",
                     "[NativeIMTranslationHandle][PPD-510]")
    {
        REQUIRE(ui.ok());
        auto floater = makeFloater();
        const LLHandle<LLFloater> handle = floater->getHandle();
        const auto completion = [handle]() {
            return static_cast<ObserverFloater*>(handle.get());
        };
        const std::function<ObserverFloater*()> copied_completion = completion;
        REQUIRE(copied_completion() == floater.get());

        floater.reset();
        CHECK(handle.isDead());
        CHECK(copied_completion() == nullptr);

        auto reopened = makeFloater();
        const LLHandle<LLFloater> new_handle = reopened->getHandle();
        CHECK(static_cast<ObserverFloater*>(new_handle.get()) == reopened.get());
        CHECK(copied_completion() == nullptr);
        CHECK(handle != new_handle);
    }
}
