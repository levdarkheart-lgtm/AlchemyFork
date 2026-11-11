/**
 * @file alscenemonitor_ui_metal_test.cpp
 * @brief Actual scene difference textures survive authoritative UI composition.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "llfontmetal_fixture.h"
#include "../llglstates.h"
#include "../llrender2dutils.h"
#include "../../almetal/almetalscenemonitor.h"
#include "../../almetal/almetalpipeline.h"
#include <array>
#include <cmath>
#include <vector>

namespace
{
using Pixel = std::array<U8, 4>;
using Backing = ALMetalRetainedBackground;

Backing::Lease frame(const ALMetalDevice& device, const Backing::Context& context,
                     bool colored, U64 index)
{
    std::string error;
    ALMetalRenderTargetDescriptor d;
    d.width = d.height = 4;
    d.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
    d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
    d.depth_stencil_shader_read = true;
    std::shared_ptr<ALMetalRenderTarget> target = ALMetalRenderTarget::create(device, d, &error);
    INFO(error); REQUIRE(target);
    // BGRA authored bytes: each 2x2 block survives every Bayer cutoff, and
    // the aggregate becomes red/green on row0, blue/yellow on row1.
    std::array<Pixel,16> pixels{};
    for (U32 y = 0; y < 4; ++y) for (U32 x = 0; x < 4; ++x)
    {
        Pixel value{0,0,0,255};
        if (colored)
        {
            if (y < 2) value = x < 2 ? Pixel{0,0,255,255} : Pixel{0,255,0,255};
            else value = x < 2 ? Pixel{255,0,0,255} : Pixel{0,255,255,255};
        }
        pixels[y*4+x] = value;
    }
    auto command = device.makeCommandBuffer("Scene monitor UI authored world", &error); REQUIRE(command);
    REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(
        *command, 0, 0, {}, pixels.data(), 4*4, &error));
    ALMetalRenderPassConfig pc;
    pc.colors[0].load = ALMetalLoadAction::load;
    pc.depth_store = pc.stencil_store = ALMetalStoreAction::store;
    pc.clear_depth = 0.0; // An ordinary mid-depth UI quad must use the original ALWAYS bracket.
    auto pass = target->makeRenderPass(pc, &error); REQUIRE(pass);
    auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Scene monitor UI depth clear", &error);
    REQUIRE(encoder); REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
    Backing owner;
    REQUIRE(owner.publish(context, index, target, target, &error));
    auto result = owner.find(context); REQUIRE(result);
    return result;
}
}

TEST_CASE("Computed scene monitor result is the actual raw native texture drawn by authoritative UI",
          "[almetal][native-scene-monitor-ui]")
{
    ll_test::NativeFontUI fixture;
    fixture.initRenderer();
    std::string error;
    const Backing::Context context{fixture.device.get(), &fixture, 1, 4, 4, false};
    auto previous = frame(*fixture.device, context, false, 1);
    auto current = frame(*fixture.device, context, true, 2);
    auto monitor = ALMetalSceneMonitor::create(*fixture.device, &error); INFO(error); REQUIRE(monitor);
    const ALMetalSceneDifferenceConfig config{4,4,4,4,0.5f,0.1f};
    gGL.matrixMode(LLRender::MM_PROJECTION); gGL.loadIdentity();
    gGL.ortho(0.f,4.f,0.f,4.f,-1.f,1.f);
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.setViewport(0,0,4,4);
    LLGLDepthTest depth(true, false, GL_ALWAYS);
    LLGLEnable blend(GL_BLEND);
    LLGLDisable cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    for (const auto blending : {LLRender::BT_REPLACE, LLRender::BT_ALPHA})
    {
        CAPTURE(static_cast<int>(blending));
        auto difference = monitor->submit(previous, current, config, &error); INFO(error); REQUIRE(difference);
        REQUIRE(fixture.device->waitUntilIdle(&error));
        U64 count = 0; F32 normalized = 0.f;
        REQUIRE(difference->result(count, normalized, &error));
        CHECK(count == 4); CHECK(std::abs(normalized-std::sqrt(0.5f)) < 1.e-6f);
        REQUIRE(difference->filteredTexture()->width() == 2);
        REQUIRE(difference->filteredTexture()->height() == 2);
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::bgra8_unorm,
            &gUIProgram, previous->colorTexture(), previous->depthTexture(), nullptr,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setSceneBlendType(blending);
        gUIProgram.bind();
        auto* slot = gGL.getTextureSlot(0);
        slot->unbind(); // Same background-to-image transition as LLSceneMonitorView.
        REQUIRE(slot->bindNative(const_cast<ALMetalTexture*>(difference->filteredTexture()),
            ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
        gGL.color4f(1.f,1.f,1.f,1.f);
        gl_rect_2d_simple_tex(2,2); gGL.flush(); slot->unbind();
        // Once encoded, the GPU must retain the actual filtered generation,
        // independently of the numeric ticket and all of its C++ textures.
        difference.reset();
        LLRender::EMetalUICaptureEndState state{};
        auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
        INFO(error); REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE); REQUIRE(output);
        std::array<Pixel,16> actual{};
        REQUIRE(output->readback(*fixture.device,0,0,{},actual.data(),4*4,&error));
        const U8 channel = blending == LLRender::BT_REPLACE ? 255 : 242;
        const U8 alpha = blending == LLRender::BT_REPLACE ? 242 : 243;
        const std::array<Pixel,4> quadrants{{
            {0,0,channel,alpha}, {0,channel,0,alpha},
            {channel,0,0,alpha}, {0,channel,channel,alpha}}};
        for (U32 y=0;y<4;++y) for (U32 x=0;x<4;++x)
        {
            CAPTURE(x,y);
            const Pixel expected = x<2 && y<2 ? quadrants[y*2+x] : Pixel{0,0,0,255};
            INFO("actual BGRA " << +actual[y*4+x][0] << "," << +actual[y*4+x][1]
                 << "," << +actual[y*4+x][2] << "," << +actual[y*4+x][3]);
            CHECK(actual[y*4+x] == expected);
        }
    }
}
