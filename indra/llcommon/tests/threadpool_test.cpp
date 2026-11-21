/**
 * @file threadpool_test.cpp
 * @brief Validated configured widths and fixed ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "threadpool.h"
#include "llevents.h"
#include "llsd.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace
{
struct WidthSettings
{
    LLSD value = LLSD::emptyMap();
    LLTempBoundListener listener;
    WidthSettings() : listener(LLEventPumps::instance().obtain("LLViewerControl").listen(
        "width settings", [this](const LLSD& request)
        {
            LLSD reply;
            reply["value"] = value;
            LLEventPumps::instance().obtain(request["reply"].asString()).post(reply);
            return false;
        })) {}
};

TEST_CASE_METHOD(WidthSettings, "thread pool: configured widths reject zero negative and excessive values", "[threadpool]")
{
    CHECK(LL::ThreadPoolBase::getConfiguredWidth("Configured", 3) == 3);
    for (const LLSD& invalid : {LLSD(0), LLSD(-1), LLSD(std::numeric_limits<S32>::max()),
                               LLSD(257), LLSD("3"), LLSD(true), LLSD(2.5)})
    {
        value["Configured"] = invalid;
        CHECK(LL::ThreadPoolBase::getConfiguredWidth("Configured", 3) == 3);
    }
    for (S32 valid : {1, 3, 16, 256})
    {
        value["Configured"] = valid;
        CHECK(LL::ThreadPoolBase::getConfiguredWidth("Configured", 3) == size_t(valid));
    }
    value["Configured"] = 0;
    LL::ThreadPool pool("Configured", 3, 1024, false);
    pool.start();
    CHECK(pool.getWidth() == 3);
    pool.close();
}

TEST_CASE_METHOD(WidthSettings, "thread pool: shipped-style selection and fixed owners keep distinct widths", "[threadpool]")
{
    value["General"] = 1;
    LL::ThreadPool general("General", 3, 1024, false);
    general.start();
    CHECK(general.getWidth() == 1);
    general.close();
    value["Upload owner"] = 99;
    LL::ThreadPool fixed("Upload owner", 1, 1024, false, true);
    fixed.start();
    CHECK(fixed.getWidth() == 1);
    fixed.close();
}
}
