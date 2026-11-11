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
#include "llgl.h"
#include "llsd.h"

namespace
{
struct DiagnosticStateScope
{
    ALRenderCapabilities capabilities = gRenderCapabilities;
    ~DiagnosticStateScope()
    {
        gRenderCapabilities = capabilities;
    }
};
}
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
    CHECK(caps.textureMemoryBudgetMB() == 6 * 1024);
    CHECK(caps.maxTextureDimension() == 16'384);
    CHECK(caps.hasAnisotropicFiltering());
    CHECK(caps.maxAnisotropy() == 16.f);
    CHECK_FALSE(caps.hasLegacyShaderLimits());

    caps.reset();
    CHECK_FALSE(caps.isInitialized());
    CHECK(caps.identity().empty());
}

TEST_CASE("ALRenderCapabilities publishes the Metal recommendation as an image capacity ceiling",
          "[alrendercapabilities][almetal]")
{
    ALMetalDeviceInfo info;
    info.name = "Test Apple GPU";

    ALRenderCapabilities caps;

    info.recommended_working_set_size = 512ull << 20;
    caps.initializeMetal(info);
    CHECK(caps.memoryBudgetMB() == 512);
    CHECK(caps.textureMemoryBudgetMB() == 512);

    info.recommended_working_set_size = 64ull << 30;
    caps.initializeMetal(info);
    CHECK(caps.memoryBudgetMB() == 64 * 1024);
    CHECK(caps.textureMemoryBudgetMB() == 64 * 1024);

    info.recommended_working_set_size = 256ull << 30;
    caps.initializeMetal(info);
    CHECK(caps.memoryBudgetMB() == 256 * 1024);
    CHECK(caps.textureMemoryBudgetMB() == 256 * 1024);

    info.recommended_working_set_size = 0;
    caps.initializeMetal(info);
    CHECK(caps.memoryBudgetMB() == 0);
    CHECK(caps.textureMemoryBudgetMB() == 0);
}

TEST_CASE("LLGLManager diagnostics report native capabilities without OpenGL queries",
          "[alrendercapabilities][almetal][native-diagnostics]")
{
    // Original1eb44's diagnostics queried the active GL context. Their
    // native equivalent must report the selected renderer even though the
    // legacy manager was never initialized, and never invent GL limits.
    REQUIRE_FALSE(gGLManager.mInited);
    DiagnosticStateScope restore;
    LLGLManager manager;
    REQUIRE_FALSE(manager.mInited);
    manager.mGLVendorShort = "stale GL vendor";
    manager.mGLRenderer = "stale GL renderer";
    manager.mGLVersionString = "stale GL version";
    manager.mVRAM = 17;
    manager.mGLMaxTextureSize = 512;

    LLSD debug_info;
    debug_info["unrelated_debug_data"] = "preserved";
    debug_info["GLInfo"]["GLVersion"] = "stale GL version";
    debug_info["RendererInfo"]["ObsoleteField"] = true;
    LLSD telemetry;
    telemetry["unrelated_telemetry"] = "preserved";
    const auto legacy_limits = {
        "opengl_version", "gl_renderer", "max_samples", "num_texture_image_units",
        "max_sample_mask_words", "max_color_texture_samples", "max_depth_texture_samples",
        "max_integer_samples", "max_vertex_range", "max_index_range", "has_compute",
        "max_compute_work_group_invocations", "max_compute_shared_memory_size",
        "max_image_units", "max_vertex_shader_storage_blocks",
        "max_fragment_shader_storage_blocks", "max_compute_shader_storage_blocks"};

    // Reuse one legacy manager and both diagnostic maps while changing the
    // authoritative native metadata. This also rejects accidental stale GL
    // field reuse or a hard-coded GPU/limit response.
    for (unsigned revision = 0; revision < 2; ++revision)
    {
        ALMetalDeviceInfo info;
        info.name = revision == 0 ? "Synthetic native diagnostic GPU A"
                                  : "Synthetic native diagnostic GPU B";
        info.registry_id = revision == 0 ? 314159 : 271828;
        info.highest_apple_family = revision == 0 ? 7 : 9;
        info.highest_mac_family = 2;
        info.recommended_working_set_size = (revision == 0 ? 7ull : 24ull) << 30;
        info.max_texture_2d_dimension = revision == 0 ? 8192 : 16384;
        gRenderCapabilities.initializeMetal(info);
        CAPTURE(revision);

        manager.getGLInfo(debug_info);
        const LLSD& renderer = debug_info["RendererInfo"];
        CHECK(renderer["Backend"].asString() == "Metal");
        CHECK(renderer["Vendor"].asString() == "Apple");
        CHECK(renderer["Renderer"].asString() == info.name);
        CHECK(renderer["Version"].asString() == gRenderCapabilities.version());
        CHECK(renderer["Identity"].asString() == gRenderCapabilities.identity());
        CHECK(renderer["MemoryBudgetMB"].asInteger() == (revision == 0 ? 7168 : 24576));
        CHECK(renderer["MaxTextureDimension"].asInteger() ==
              static_cast<LLSD::Integer>(info.max_texture_2d_dimension));
        CHECK_FALSE(renderer.has("ObsoleteField"));
        CHECK_FALSE(debug_info.has("GLInfo"));
        CHECK(debug_info["unrelated_debug_data"].asString() == "preserved");

        const std::string text = manager.getGLInfoString();
        CHECK(text.find("Metal") != std::string::npos);
        CHECK(text.find(info.name) != std::string::npos);
        CHECK(text.find(gRenderCapabilities.identity()) != std::string::npos);
        CHECK(text.find("GL_VENDOR") == std::string::npos);
        CHECK(text.find("GL_RENDERER") == std::string::npos);
        CHECK(text.find("GL_VERSION") == std::string::npos);
        CHECK(text.find("GL_EXTENSIONS") == std::string::npos);
        CHECK(text.find("stale") == std::string::npos);
        CHECK(manager.getRawGLString() == gRenderCapabilities.identity());
        manager.printGLInfoString();

        for (const auto* key : legacy_limits) telemetry[key] = "stale GL value";
        manager.asLLSD(telemetry);
        CHECK(telemetry["renderer_backend"].asString() == "Metal");
        CHECK(telemetry["gpu_vendor"].asString() == "Apple");
        CHECK(telemetry["gpu_version"].asString() == gRenderCapabilities.version());
        CHECK(telemetry["renderer"].asString() == info.name);
        CHECK(telemetry["gpu_driver"].asString() == gRenderCapabilities.identity());
        CHECK(telemetry["vram"].asInteger() == (revision == 0 ? 7168 : 24576));
        CHECK(telemetry["max_texture_size"].asInteger() ==
              static_cast<LLSD::Integer>(info.max_texture_2d_dimension));
        CHECK_FALSE(telemetry["is_ati"].asBoolean());
        CHECK_FALSE(telemetry["is_nvidia"].asBoolean());
        CHECK_FALSE(telemetry["is_intel"].asBoolean());
        for (const auto* key : legacy_limits)
        {
            CAPTURE(key);
            CHECK_FALSE(telemetry.has(key));
        }
        CHECK(telemetry["unrelated_telemetry"].asString() == "preserved");
        CHECK_FALSE(manager.mInited);
        CHECK_FALSE(gGLManager.mInited);
    }

    // Startup failure reporting can precede native device publication. An
    // absent renderer must stay unknown, with neither a GL query nor a
    // fabricated Metal device/budget carried over from a previous report.
    gRenderCapabilities.reset();
    debug_info["GLInfo"]["GLVersion"] = "stale GL version";
    manager.getGLInfo(debug_info);
    const LLSD& unavailable = debug_info["RendererInfo"];
    CHECK(unavailable["Backend"].asString() == "none");
    for (const auto* key : {"Vendor", "Renderer", "Version", "Identity",
                            "MemoryBudgetMB", "MaxTextureDimension"})
    {
        CAPTURE(key);
        CHECK_FALSE(unavailable.has(key));
    }
    CHECK_FALSE(debug_info.has("GLInfo"));
    CHECK(debug_info["unrelated_debug_data"].asString() == "preserved");
    CHECK(manager.getGLInfoString() == "RENDERER_BACKEND none\n");
    CHECK(manager.getRawGLString() == "Renderer unavailable");
    manager.printGLInfoString();

    for (const auto* key : legacy_limits) telemetry[key] = "stale GL value";
    manager.asLLSD(telemetry);
    CHECK(telemetry["renderer_backend"].asString() == "none");
    for (const auto* key : {"gpu_vendor", "gpu_version", "renderer", "gpu_driver",
                            "vram", "max_texture_size", "is_ati", "is_nvidia", "is_intel"})
    {
        CAPTURE(key);
        CHECK_FALSE(telemetry.has(key));
    }
    for (const auto* key : legacy_limits)
    {
        CAPTURE(key);
        CHECK_FALSE(telemetry.has(key));
    }
    CHECK(telemetry["unrelated_telemetry"].asString() == "preserved");
    CHECK_FALSE(manager.mInited);
    CHECK_FALSE(gGLManager.mInited);
}
#endif
