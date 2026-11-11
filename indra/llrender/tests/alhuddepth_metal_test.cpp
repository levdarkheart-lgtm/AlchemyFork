/**
 * @file alhuddepth_metal_test.cpp
 * @brief Original HUD clear/restore helpers through the authoritative native UI.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "llfontmetal_fixture.h"
#include "../llrendertarget.h"
#include "../../alrender/alhuddepth.h"
#include "../../almetal/almetalpipeline.h"
#include <array>

namespace
{
using Pixel = std::array<U8, 4>;
struct Reverse
{
    bool previous = LLRender::sReverseZ;
    explicit Reverse(bool value) { LLRender::sReverseZ = value; }
    ~Reverse() { LLRender::sReverseZ = previous; }
};
struct HUD : ll_test::NativeFontUI
{
    static constexpr S32 width = 16, height = 24;
    LLGLSLShader copy;
    LLRenderTarget scene;
    HUD()
    {
        initRenderer();
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadIdentity(); gGL.ortho(0, width, 0, height, -1, 1);
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        copy.mName = "Original copy-depth helper policy";
        copy.mShaderFiles = {{"interface/copyV.glsl", GL_VERTEX_SHADER},
                             {"interface/copyF.glsl", GL_FRAGMENT_SHADER}};
        copy.addPermutation("COPY_DEPTH", "1");
        copy.addPermutation("DEPTH_ONLY", "1");
        REQUIRE(copy.createShader());
        REQUIRE(copy.isAvailable());
        REQUIRE(scene.allocate(width, height, GL_RGBA8, true, false,
            ALTextureSlot::TT_TEXTURE, LLRenderTarget::MIPS_NONE, LLRenderTarget::DEPTH_FMT_32F));
        std::string error;
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {0,0,0,1};
        clear.clear_depth = LLRender::sReverseZ ? .75 : .25;
        clear.depth_store = ALMetalStoreAction::store;
        auto pass = scene.getMetalRenderTarget()->makeRenderPass(clear, &error); REQUIRE(pass);
        auto command = device->makeCommandBuffer("HUD authored near-world backing", &error); REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "HUD backing clear", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
    }
    void begin()
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
            &gUIProgram, scene.getMetalColorTexture(), scene.getMetalDepthTexture(),
            nullptr, LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setViewport(0, 0, width, height);
        gUIProgram.bind(); gGL.getTextureSlot(0)->unbind();
    }
    void draw(Pixel color, LLRect rect, F32 z = 0, bool flush = true)
    {
        gUIProgram.bind();
        gGL.color4ub(color[0], color[1], color[2], color[3]);
        gGL.begin(LLRender::TRIANGLES);
        for (auto p : std::array<std::array<F32,2>,6>{{
            {F32(rect.mLeft),F32(rect.mBottom)}, {F32(rect.mRight),F32(rect.mBottom)},
            {F32(rect.mRight),F32(rect.mTop)}, {F32(rect.mLeft),F32(rect.mBottom)},
            {F32(rect.mRight),F32(rect.mTop)}, {F32(rect.mLeft),F32(rect.mTop)}}})
            gGL.vertex3f(p[0], p[1], z);
        gGL.end();
        if (flush) gGL.flush();
    }
    void probe(Pixel color) { draw(color, LLRect(0,height,width,0)); }
    std::vector<Pixel> finish()
    {
        // End capture consumes encoded batches. Submit the producer's final
        // pending immediate batch explicitly, as the viewer's UI boundary does.
        gGL.flush();
        std::string error;
        LLRender::EMetalUICaptureEndState state{};
        auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
        INFO(error); REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
        REQUIRE(output);
        std::vector<Pixel> result(width * height);
        REQUIRE(output->readback(*device, 0, 0, {}, result.data(), width * 4, &error));
        return result;
    }
};
}

TEST_CASE("HUD depth helper clears the original asymmetric lower rows only",
          "[almetal][native-hud-depth][hud-clear-rows]")
{
    for (bool convention : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LESS);
        const LLRect rect(2, 7, 12, 2);
        hud.begin();
        ALHUDDepth::clear(hud.copy, {rect}, 0, 0);
        hud.probe({255,0,0,255});
        const auto actual = hud.finish();
        for (S32 y=0; y<HUD::height; ++y) for (S32 x=0; x<HUD::width; ++x)
        {
            CAPTURE(convention,x,y);
            const bool inside = x>=rect.mLeft && x<rect.mRight && y>=rect.mBottom && y<rect.mTop;
            CHECK(actual[y*HUD::width+x] == (inside ? Pixel{255,0,0,255} : Pixel{0,0,0,255}));
        }
    }
}

TEST_CASE("HUD restore helper keeps the actual native UI transaction valid",
          "[almetal][native-hud-depth][hud-restore-admission]")
{
    for (bool convention : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        hud.begin();
        ALHUDDepth::restore(hud.copy, {LLRect(2,7,12,2)}, 0, 0);
        CHECK(gGL.hasMetalUITextureCapture());
        const auto actual = hud.finish();
        for (const auto& pixel : actual) CHECK(pixel == Pixel{0,0,0,255});
    }
}

TEST_CASE("HUD bracket retains the final world's color and exact copied depth",
          "[almetal][native-hud-depth][hud-depth-copy]")
{
    for (bool convention : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        hud.begin();
        {
            // A point farther than the world must fail before any bracket.
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LESS);
            hud.probe({255,0,0,255});
        }
        auto actual = hud.finish();
        for (const auto& pixel : actual) CHECK(pixel == Pixel{0,0,0,255});

        hud.begin();
        ALHUDDepth::clear(hud.copy, {LLRect(2,7,12,2)}, 0, 0);
        ALHUDDepth::restore(hud.copy, {LLRect(2,7,12,2)}, 0, 0);
        {
            // Ortho maps z=.5 to .25 forward/.75 reverse: an equality
            // probe proves the copied and restored scene depth, not merely
            // an arbitrary occluding value, throughout the complete target.
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_EQUAL);
            hud.draw({0,255,0,255}, LLRect(0,HUD::height,HUD::width,0), .5f);
        }
        actual = hud.finish();
        for (const auto& pixel : actual) CHECK(pixel == Pixel{0,255,0,255});
        std::vector<F32> scene_depth(HUD::width*HUD::height);
        std::string error;
        REQUIRE(hud.scene.getMetalDepthTexture()->readback(*hud.device,0,0,{},
            scene_depth.data(),HUD::width*sizeof(F32),&error));
        for (F32 value : scene_depth) CHECK(value == (convention ? .75f : .25f));
    }
}

TEST_CASE("HUD restore hides farther UI and preserves nearer HUD depth",
          "[almetal][native-hud-depth][hud-conditional-restore]")
{
    for (bool convention : {false,true}) for (bool restore : {false,true})
        for (bool front_of_world : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        const LLRect region(2,10,14,2), attachment(2,7,6,2);
        hud.begin();
        ALHUDDepth::clear(hud.copy, {region}, 0, 0);
        {
            LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_LESS);
            // .125 forward/.875 reverse is nearer than the world. Leave
            // it queued: restore must flush this exact batch before itself.
            hud.draw({255,255,255,255},attachment,.75f,false);
            if (restore) ALHUDDepth::restore(hud.copy, {region}, 0, 0);
        }
        {
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LESS);
            // Either behind the world or between world and nearer HUD.
            hud.draw({255,255,0,255},region,front_of_world ? .6f : .25f);
        }
        const auto actual = hud.finish();
        for (S32 y=0; y<HUD::height; ++y) for (S32 x=0; x<HUD::width; ++x)
        {
            CAPTURE(convention,restore,front_of_world,x,y);
            const bool in_region=x>=2 && x<14 && y>=2 && y<10;
            const bool on_hud=x>=2 && x<6 && y>=2 && y<7;
            Pixel expected{0,0,0,255};
            if (on_hud) expected={255,255,255,255};
            else if (in_region && (!restore || front_of_world)) expected={255,255,0,255};
            CHECK(actual[y*HUD::width+x] == expected);
        }
    }
}

TEST_CASE("HUD rectangles honor viewport offsets clipping gaps and empty bounds",
          "[almetal][native-hud-depth][hud-rect-offsets]")
{
    for (bool convention : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        hud.begin();
        // Target-space rectangles are [-2,3)x[3,7), [8,11)x[4,8),
        // wholly outside, and empty. The two active regions stay separate.
        ALHUDDepth::clear(hud.copy, {LLRect(-4,4,1,0),LLRect(6,5,9,1),
                                   LLRect(20,3,25,0),LLRect(1,1,1,1)}, 2,3);
        hud.probe({255,0,0,255});
        const auto actual=hud.finish();
        for (S32 y=0; y<HUD::height; ++y) for (S32 x=0; x<HUD::width; ++x)
        {
            CAPTURE(convention,x,y);
            const bool inside=(x<3 && y>=3 && y<7) || (x>=8 && x<11 && y>=4 && y<8);
            CHECK(actual[y*HUD::width+x] == (inside ? Pixel{255,0,0,255} : Pixel{0,0,0,255}));
        }
    }
}

TEST_CASE("HUD clear orders queued geometry before changing depth without changing logical state",
          "[almetal][native-hud-depth][hud-clear-order]")
{
    for (bool convention : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        hud.begin();
        // Queued farther red must test against the old world, never a later
        // clear. No state change is available to incidentally flush it.
        hud.draw({255,0,0,255},LLRect(0,HUD::height,HUD::width,0),0,false);
        auto* shader=LLGLSLShader::sCurBoundShaderPtr;
        bool mask[4]; gGL.getColorMask(mask);
        ALHUDDepth::clear(hud.copy,{LLRect(2,7,12,2)},0,0);
        CHECK(LLGLSLShader::sCurBoundShaderPtr == shader);
        CHECK(LLGLDepthTest::depthEnabled());
        CHECK_FALSE(LLGLDepthTest::depthWriteEnabled());
        CHECK(LLGLDepthTest::depthFunction() == GL_LESS);
        bool after[4]; gGL.getColorMask(after);
        for (U32 i=0;i<4;++i) CHECK(after[i]==mask[i]);
        const auto actual=hud.finish();
        for (const auto& pixel : actual) CHECK(pixel == Pixel{0,0,0,255});
    }
}

TEST_CASE("HUD without an available copy identity leaves the world's occlusion intact",
          "[almetal][native-hud-depth][hud-no-copy]")
{
    for (bool convention : {false,true})
    {
        Reverse reverse(convention);
        HUD hud;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        LLGLSLShader unloaded;
        REQUIRE_FALSE(unloaded.isAvailable());
        hud.begin();
        ALHUDDepth::clear(unloaded,{LLRect(0,HUD::height,HUD::width,0)},0,0);
        ALHUDDepth::restore(unloaded,{LLRect(0,HUD::height,HUD::width,0)},0,0);
        hud.probe({255,0,0,255});
        const auto actual=hud.finish();
        for (const auto& pixel : actual) CHECK(pixel == Pixel{0,0,0,255});
    }
}

TEST_CASE("HUD boxRect covers a box's pixels grown by one and clipped to the viewport",
          "[almetal][native-hud-depth][hud-box]")
{
    constexpr S32 W=64,H=64;
    const LLMatrix4a ortho=LLMatrix4a::ortho(-.5f,.5f,-.5f,.5f,0,10);
    CHECK(ALHUDDepth::boxRect(ortho,LLVector3(-.25f,-.25f,-2),LLVector3(.25f,.25f,0),W,H)==LLRect(15,49,49,15));
    CHECK(ALHUDDepth::boxRect(ortho,LLVector3(-.1f,0,-1),LLVector3(.13f,.2f,-1),W,H)==LLRect(24,46,42,31));
    CHECK(ALHUDDepth::boxRect(ortho,LLVector3(.4f,-.1f,-1),LLVector3(.8f,.1f,-1),W,H)==LLRect(56,40,64,24));
    CHECK(ALHUDDepth::boxRect(ortho,LLVector3(.6f,-.1f,-1),LLVector3(.9f,.1f,-1),W,H).isEmpty());
}

TEST_CASE("HUD mergeRect folds overlaps and keeps disjoint bounds apart",
          "[almetal][native-hud-depth][hud-merge]")
{
    std::vector<LLRect> rects;
    ALHUDDepth::mergeRect(rects,LLRect(0,10,10,0));
    ALHUDDepth::mergeRect(rects,LLRect(5,15,15,5));
    REQUIRE(rects.size()==1);
    CHECK(rects[0]==LLRect(0,15,15,0));
    ALHUDDepth::mergeRect(rects,LLRect(30,40,40,30));
    REQUIRE(rects.size()==2);
    ALHUDDepth::mergeRect(rects,LLRect(14,31,31,14));
    REQUIRE(rects.size()==1);
    CHECK(rects[0]==LLRect(0,40,40,0));
}
