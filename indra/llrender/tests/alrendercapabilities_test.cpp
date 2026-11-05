/**
 * @file alrendercapabilities_test.cpp
 * @brief Unit tests for backend-neutral renderer capabilities.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "catch2/catch_test_macros.hpp"

#include "alrendercapabilities.h"

#if LL_DARWIN
#include "almetaldevice.h"
#endif

TEST_CASE("ALRenderCapabilities resets to no backend", "[alrendercapabilities]")
{
    ALRenderCapabilities caps;
    CHECK_FALSE(caps.isInitialized());
    CHECK_FALSE(caps.isMetal());
    CHECK(caps.backend() == ALRendererBackend::none);
    CHECK(caps.maxAnisotropy() == 1.f);
}

#if LL_DARWIN
TEST_CASE("ALRenderCapabilities publishes Metal identity without claiming GL initialization",
          "[alrendercapabilities][almetal]")
{
    ALMetalDeviceInfo info;
    info.name = "Test Apple GPU";
    info.registry_id = 42;
    info.recommended_working_set_size = 6ull << 30;
    info.max_texture_2d_dimension = 16'384;
    info.highest_apple_family = 8;
    info.highest_mac_family = 2;

    ALRenderCapabilities caps;
    caps.initializeMetal(info);

    CHECK(caps.isInitialized());
    CHECK(caps.isMetal());
    CHECK(caps.backend() == ALRendererBackend::metal);
    CHECK(caps.vendor() == "Apple");
    CHECK(caps.renderer() == info.name);
    CHECK(caps.identity().find("registry 42") != std::string::npos);
    CHECK(caps.version().find("Apple GPU family 8") != std::string::npos);
    CHECK(caps.isApple());
    CHECK_FALSE(caps.isAMD());
    CHECK(caps.memoryBudgetMB() == 6 * 1024);
    CHECK(caps.maxTextureDimension() == 16'384);
    CHECK(caps.hasAnisotropicFiltering());
    CHECK(caps.maxAnisotropy() == 16.f);
    CHECK_FALSE(caps.hasLegacyShaderLimits());

    caps.reset();
    CHECK_FALSE(caps.isInitialized());
    CHECK(caps.identity().empty());
}
#endif
