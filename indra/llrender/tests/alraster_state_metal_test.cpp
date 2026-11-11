/**
 * @file alraster_state_metal_test.cpp
 * @brief Original logical raster state reaches the ordered native UI owner.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include "../llrendertarget.h"
#include "../../almetal/almetalpipeline.h"
#include "../../almetal/almetaluirenderer.h"
#include <array>
#include <limits>

namespace
{
using Pixel = std::array<U8,4>;
constexpr Pixel black{0,0,0,255}, red{255,0,0,255}, green{0,255,0,255};
struct Convention
{
    bool old=LLRender::sReverseZ;
    explicit Convention(bool reverse) { LLRender::sReverseZ=reverse; }
    ~Convention() { LLRender::sReverseZ=old; }
};
struct Raster : ll_test::NativeFontUI
{
    static constexpr S32 width=32,height=24;
    LLRenderTarget scene;
    F32 old_slope=gGL.getPolygonOffsetFactor(),old_units=gGL.getPolygonOffsetUnits();
    U32 old_cull=gGL.getCullFace(),old_rgb=0,old_alpha=0;
    LLColor4 old_clear=gGL.getClearColor();
    Raster()
    {
        gGL.getBlendEquation(old_rgb,old_alpha);
        initRenderer();
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadIdentity();gGL.ortho(0,width,0,height,-1,1);
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        REQUIRE(scene.allocate(width,height,GL_RGBA8,true,false,ALTextureSlot::TT_TEXTURE,
            LLRenderTarget::MIPS_NONE,LLRenderTarget::DEPTH_FMT_32F));
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear={0,0,0,1};
        clear.clear_depth=.5;
        clear.depth_store=ALMetalStoreAction::store;
        std::string error;
        auto pass=scene.getMetalRenderTarget()->makeRenderPass(clear,&error);REQUIRE(pass);
        auto command=device->makeCommandBuffer("Raster invariant depth backing",&error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Raster invariant backing",&error);REQUIRE(encoder);
        REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));
    }
    ~Raster()
    {
        gGL.setPolygonOffset(old_slope,old_units);gGL.setCullFace(old_cull);
        gGL.setBlendEquation(old_rgb,old_alpha);gGL.setClearColor(old_clear);
    }
    void begin(bool retain_depth=true,bool retain_color=true)
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,&gUIProgram,
            retain_color?scene.getMetalColorTexture():nullptr,
            retain_depth?scene.getMetalDepthTexture():nullptr,nullptr,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setViewport(0,0,width,height);
        gUIProgram.bind();gGL.getTextureSlot(0)->unbind();
    }
    void quad(Pixel color,S32 left=0,S32 right=width,F32 left_z=0,F32 right_z=0,bool flush=true)
    {
        gGL.color4ub(color[0],color[1],color[2],color[3]);
        gGL.begin(LLRender::TRIANGLES);
        gGL.vertex3f(left,0,left_z);gGL.vertex3f(right,0,right_z);gGL.vertex3f(right,height,right_z);
        gGL.vertex3f(left,0,left_z);gGL.vertex3f(right,height,right_z);gGL.vertex3f(left,height,left_z);
        gGL.end();if(flush)gGL.flush();
    }
    std::vector<Pixel> finish(U32 output_width=width,U32 output_height=height)
    {
        gGL.flush();
        std::string error;
        LLRender::EMetalUICaptureEndState state{};
        auto output=gGL.endMetalUITextureCapture(true,false,nullptr,&error,&state);
        INFO(error);REQUIRE(state==LLRender::EMetalUICaptureEndState::COMPLETE);REQUIRE(output);
        REQUIRE(output->descriptor().width==output_width);
        REQUIRE(output->descriptor().height==output_height);
        std::vector<Pixel> pixels(output_width*output_height);
        REQUIRE(output->readback(*device,0,0,{},pixels.data(),output_width*4,&error));
        return pixels;
    }
};
}

TEST_CASE("Native immediate polygon offset keeps the original toward-camera sign",
          "[almetal][native-raster-state][raster-offset]")
{
    // Original LLManipScale::render enables POLYGON_OFFSET_FILL and uses
    // (-2,-2) while depth-tested axis boxes are drawn. LLRender flips that
    // logical sign for reverse Z. A larger constant isolates its direction
    // from implementation-dependent minimum representable depth offsets.
    for(bool reverse:{false,true})for(bool enabled:{false,true})for(F32 units:{-16.f,0.f,16.f})
    {
        Convention convention(reverse);Raster raster;
        LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLState offset(GL_POLYGON_OFFSET_FILL,enabled);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        raster.begin();gGL.setPolygonOffset(0,units);raster.quad(red);
        const auto actual=raster.finish();
        for(const auto& pixel:actual)
        {
            CAPTURE(reverse,enabled,units);
            CHECK(pixel==(enabled&&units<0?red:black));
        }
    }
}

TEST_CASE("Native immediate polygon offset flushes pending draws under their old values",
          "[almetal][native-raster-state][raster-offset-order]")
{
    for(bool reverse:{false,true})
    {
        Convention convention(reverse);Raster raster;
        LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        raster.begin();gGL.setPolygonOffset(0,-16);
        raster.quad(red,0,Raster::width/2,0,0,false);
        // Same shader/depth state: only this setter can order the red batch
        // under its negative offset before the positive-offset green batch.
        gGL.setPolygonOffset(0,16);
        raster.quad(green,Raster::width/2);
        const auto actual=raster.finish();
        for(S32 y=0;y<Raster::height;++y)for(S32 x=0;x<Raster::width;++x)
        {
            CAPTURE(reverse,x,y);
            CHECK(actual[y*Raster::width+x]==(x<Raster::width/2?red:black));
        }
    }
}

TEST_CASE("Native immediate slope offset uses window depth slope for both conventions",
          "[almetal][native-raster-state][raster-offset-slope]")
{
    for(bool reverse:{false,true})
    {
        Convention convention(reverse);Raster raster;
        LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        raster.begin();gGL.setPolygonOffset(-2,0);
        // D=.25+.5*x/32. The original factor -2 moves D toward camera by
        // twice max(abs(dD/dx),abs(dD/dy))=.03125. Pixel centers x<18 pass.
        raster.quad(red,0,Raster::width,.5f,-.5f);
        const auto actual=raster.finish();
        for(S32 y=0;y<Raster::height;++y)for(S32 x=0;x<Raster::width;++x)
        {
            CAPTURE(reverse,x,y);
            CHECK(actual[y*Raster::width+x]==(x<18?red:black));
        }
    }
}

TEST_CASE("Native indexed HUD offset keeps the original emboss toward-camera sign",
          "[almetal][native-raster-state][raster-hud-offset]")
{
    // Original LLDrawPoolBump::renderBump uses (-1,-1); native HUD captures
    // those exact logical values. Its typed encoder must agree with the
    // immediate scale-handle path, including reverse Z.
    for(bool reverse:{false,true})for(F32 units:{-16.f,0.f,16.f})
    {
        Convention convention(reverse);Raster raster;
        std::string error;
        const float z=reverse?.5f:0.f;
        const std::array<std::array<float,4>,3> positions{{{-1,-1,z,1},{3,-1,z,1},{-1,3,z,1}}};
        const std::array<U16,3> indices{{0,1,2}};
        ALMetalBufferDescriptor desc;desc.length=sizeof(positions);
        auto vertices=ALMetalBuffer::create(*raster.device,desc,&error);REQUIRE(vertices);
        REQUIRE(vertices->write(0,positions.data(),sizeof(positions),&error));
        desc.length=sizeof(indices);
        auto index=ALMetalBuffer::create(*raster.device,desc,&error);REQUIRE(index);
        REQUIRE(index->write(0,indices.data(),sizeof(indices),&error));
        auto renderer=ALMetalUIRenderer::createForTexture(*raster.device,*raster.frames,
            ALMetalPixelFormat::rgba8_unorm,&error);REQUIRE(renderer);
        REQUIRE(renderer->setFrameBackground(raster.scene.getMetalColorTexture(),&error));
        REQUIRE(renderer->setFrameDepth(raster.scene.getMetalDepthTexture(),&error));
        ALMetalHUDIndexedBatch batch;
        batch.vertices=vertices.get();batch.vertex_count=3;
        batch.indices=index.get();batch.index_count=3;
        batch.shader=ALMetalHUDShader::debug;
        batch.blend.enabled=false;
        batch.highlight_color[0]=1;batch.highlight_color[1]=0;batch.highlight_color[2]=0;batch.highlight_color[3]=1;
        batch.depth.enabled=true;batch.depth.write_enabled=false;batch.depth.reverse_z=reverse;
        batch.depth.compare=reverse?ALMetalCompareFunction::greater:ALMetalCompareFunction::less;
        batch.depth_bias_units=units;
        batch.target_width=Raster::width;batch.target_height=Raster::height;
        batch.viewport={0,0,Raster::width,Raster::height,0,1};
        batch.scissor={0,0,Raster::width,Raster::height};
        for(U32 i=0;i<4;++i)batch.matrices.modelview_projection[i*5]=1;
        REQUIRE(renderer->drawHUDIndexed(batch,&error));
        REQUIRE(renderer->endFrame(&error));
        REQUIRE(raster.frames->waitUntilIdle(5000,&error));
        const auto* output=renderer->outputTexture();REQUIRE(output);
        std::vector<Pixel> actual(Raster::width*Raster::height);
        REQUIRE(output->readback(*raster.device,0,0,{},actual.data(),Raster::width*4,&error));
        for(const auto& pixel:actual)
        {
            CAPTURE(reverse,units);
            CHECK(pixel==(units<0?red:black));
        }
    }
}

TEST_CASE("Native polygon offset disabling restores unbiased draws in the same encoder",
          "[almetal][native-raster-state][raster-offset-reset]")
{
    for(bool reverse:{false,true})
    {
        Convention convention(reverse);Raster raster;
        LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLState offset(GL_POLYGON_OFFSET_FILL,LLGLState::ENABLED_STATE);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LESS);
        raster.begin();gGL.setPolygonOffset(0,-16);
        raster.quad(red,0,Raster::width/2,0,0,false);
        // Disable must drain the old enabled batch, then the native encoder
        // must explicitly clear bias for the otherwise identical next draw.
        offset.disable();
        raster.quad(green,Raster::width/2);
        const auto actual=raster.finish();
        for(S32 y=0;y<Raster::height;++y)for(S32 x=0;x<Raster::width;++x)
        {
            CAPTURE(reverse,x,y);
            CHECK(actual[y*Raster::width+x]==(x<Raster::width/2?red:black));
        }
    }
}

TEST_CASE("Native UI rejects nonfinite bias before opening a frame and accepts the next valid draw",
          "[almetal][native-raster-state][raster-offset-invalid]")
{
    Raster raster;
    std::string error;
    auto renderer=ALMetalUIRenderer::createForTexture(*raster.device,*raster.frames,
        ALMetalPixelFormat::rgba8_unorm,&error);REQUIRE(renderer);
    const std::array<std::array<float,4>,3> positions{{{-1,-1,0,1},{3,-1,0,1},{-1,3,0,1}}};
    ALMetalBufferDescriptor desc;desc.length=sizeof(positions);
    auto vertices=ALMetalBuffer::create(*raster.device,desc,&error);REQUIRE(vertices);
    REQUIRE(vertices->write(0,positions.data(),sizeof(positions),&error));
    ALMetalUIBatch batch;
    batch.vertices=vertices.get();batch.vertex_count=3;
    batch.shader=ALMetalUIShader::uniform_color;
    batch.blend.enabled=false;
    batch.solid_color[0]=1;batch.solid_color[1]=0;batch.solid_color[2]=0;batch.solid_color[3]=1;
    batch.target_width=Raster::width;batch.target_height=Raster::height;
    batch.viewport={0,0,Raster::width,Raster::height,0,1};
    batch.scissor={0,0,Raster::width,Raster::height};
    for(U32 i=0;i<4;++i)batch.matrices.modelview_projection[i*5]=1;
    batch.depth_bias_slope=std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(renderer->draw(batch,&error));
    CHECK(error=="Metal UI depth bias must be finite");
    batch.depth_bias_slope=0;batch.depth_bias_units=std::numeric_limits<float>::infinity();
    error.clear();
    CHECK_FALSE(renderer->draw(batch,&error));
    CHECK(error=="Metal UI depth bias must be finite");
    CHECK(renderer->stats().frames==0);
    CHECK(renderer->outputTexture()==nullptr);
    batch.depth_bias_units=0;error.clear();
    REQUIRE(renderer->draw(batch,&error));REQUIRE(renderer->endFrame(&error));
    REQUIRE(raster.frames->waitUntilIdle(5000,&error));
    REQUIRE(renderer->outputTexture());
    std::vector<Pixel> actual(Raster::width*Raster::height);
    REQUIRE(renderer->outputTexture()->readback(*raster.device,0,0,{},actual.data(),Raster::width*4,&error));
    for(const auto& pixel:actual)CHECK(pixel==red);
}

TEST_CASE("Native retained-depth immediate viewport keeps the full backing dimensions",
          "[almetal][native-raster-state][raster-viewport]")
{
    for(bool retain_depth:{false,true})
    {
        Raster raster;
        LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_FALSE);
        raster.begin(retain_depth);
        // The original framebuffer keeps its complete attachments while the
        // viewport maps geometry into a bounded subregion, independently of size.
        gGL.setViewport(3,2,16,12);
        raster.quad(red);
        const auto actual=raster.finish();
        for(S32 y=0;y<Raster::height;++y)for(S32 x=0;x<Raster::width;++x)
        {
            CAPTURE(retain_depth,x,y);
            const bool inside=x>=3&&x<19&&y>=2&&y<14;
            CHECK(actual[y*Raster::width+x]==(inside?red:black));
        }
        // Reuse this capture owner without any retained backing. Its former
        // 32x24 descriptor must not leak into a new 8x6 producer transaction.
        raster.begin(false,false);gGL.setViewport(0,0,8,6);raster.quad(red);
        const auto small=raster.finish(8,6);
        for(const auto& pixel:small)CHECK(pixel==red);
    }
}

TEST_CASE("Native target clear and nested binding preserve color and viewport ownership",
          "[almetal][native-raster-state][raster-target-state]")
{
    Raster raster;
    struct Bound
    {
        LLRenderTarget& target;
        explicit Bound(LLRenderTarget& value):target(value){target.bindTarget();}
        ~Bound(){if(target.isBoundInStack())target.flush();}
    };
    const auto viewport=[](S32 x,S32 y,S32 width,S32 height)
    {
        S32 actual[4];gGL.getViewport(actual);
        CHECK(actual[0]==x);CHECK(actual[1]==y);CHECK(actual[2]==width);CHECK(actual[3]==height);
    };
    {
        Bound bound(raster.scene);
        viewport(0,0,Raster::width,Raster::height);
        for(const auto& color:{LLColor4::red,LLColor4::green})
        {
            gGL.setClearColor(color);raster.scene.clear(GL_COLOR_BUFFER_BIT);
            std::vector<Pixel> actual(Raster::width*Raster::height);
            std::string error;
            REQUIRE(raster.scene.getMetalColorTexture()->readback(*raster.device,0,0,{},
                actual.data(),Raster::width*4,&error));
            for(const auto& pixel:actual)CHECK(pixel==(color==LLColor4::red?red:green));
        }
        LLRenderTarget nested;
        REQUIRE(nested.allocate(8,6,GL_RGBA8));
        {Bound child(nested);viewport(0,0,8,6);}
        CHECK(LLRenderTarget::getCurrentBoundTarget()==&raster.scene);
        viewport(0,0,Raster::width,Raster::height);
    }
    viewport(gGLViewport[0],gGLViewport[1],gGLViewport[2],gGLViewport[3]);
}

TEST_CASE("Native nested scissor and refresh use the retained logical clip without a driver cache",
          "[almetal][native-raster-state][raster-scissor-state]")
{
    Raster raster;
    LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    const auto box=[](S32 x,S32 y,S32 width,S32 height)
    {
        S32 actual[4];gGL.getScissor(actual);
        CHECK(actual[0]==x);CHECK(actual[1]==y);CHECK(actual[2]==width);CHECK(actual[3]==height);
    };
    raster.begin();gGL.setScissor(0,0,Raster::width,Raster::height);raster.quad(red);
    {
        LLGLSScissor left(0,0,Raster::width/2,Raster::height);
        CHECK(LLGLState::isEnabled(GL_SCISSOR_TEST));
        raster.quad(green,0,Raster::width,0,0,false);
        gGL.refreshState();
        box(0,0,Raster::width/2,Raster::height);
        {
            LLGLSScissor corner(0,0,1,1);
            box(0,0,1,1);raster.quad({0,0,255,255});
        }
        box(0,0,Raster::width/2,Raster::height);
        CHECK(LLGLState::isEnabled(GL_SCISSOR_TEST));
    }
    CHECK_FALSE(LLGLState::isEnabled(GL_SCISSOR_TEST));
    box(0,0,Raster::width,Raster::height);
    const auto actual=raster.finish();
    for(S32 y=0;y<Raster::height;++y)for(S32 x=0;x<Raster::width;++x)
    {
        CAPTURE(x,y);
        const Pixel wanted=x==0&&y==0?Pixel{0,0,255,255}:x<Raster::width/2?green:red;
        CHECK(actual[y*Raster::width+x]==wanted);
    }
}

TEST_CASE("Native raster state changes encode pending geometry once and redundant sets leave it queued",
          "[almetal][native-raster-state][raster-state-order]")
{
    Raster raster;
    LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    raster.begin();
    const auto boundary=[&](const auto& change,bool flush)
    {
        const U32 before=LLRender::sDrawCalls;
        raster.quad(red,0,Raster::width,0,0,false);
        change();
        CHECK(LLRender::sDrawCalls==before+(flush?1:0));
        gGL.flush();CHECK(LLRender::sDrawCalls==before+1);
    };
    boundary([]{gGL.setCullFace(gGL.getCullFace());},false);
    boundary([]{gGL.setCullFace(GL_FRONT);},true);
    boundary([]{gGL.setBlendEquation(GL_FUNC_ADD,GL_FUNC_ADD);},false);
    boundary([]{gGL.setBlendEquation(GL_FUNC_ADD,GL_MAX);},true);
    boundary([]{gGL.setViewport(0,0,Raster::width,Raster::height);},false);
    boundary([]{gGL.setViewport(0,0,Raster::width/2,Raster::height/2);},true);
    // The final pre-change batch still covered the complete original viewport.
    const auto actual=raster.finish();
    for(const auto& pixel:actual)CHECK(pixel==red);
}

TEST_CASE("Native renderer diagnostics retain the selected physical device name and API identity",
          "[almetal][native-raster-state][raster-device-identity]")
{
    Raster raster;
    const auto& device=raster.device->info();
    REQUIRE_FALSE(device.name.empty());
    CHECK(gRenderCapabilities.renderer()==device.name);
    CHECK(gRenderCapabilities.vendor()=="Apple");
    CHECK(gRenderCapabilities.identity().find(device.name)!=std::string::npos);
    CHECK(gRenderCapabilities.identity().find("registry "+std::to_string(device.registry_id))!=std::string::npos);
    CHECK_FALSE(gRenderCapabilities.version().empty());
    CHECK(gRenderCapabilities.isMetal());
    CHECK_FALSE(gGLManager.mInited);
}

TEST_CASE("Native localized depth clear and restore ignore a preceding polygon offset",
          "[almetal][native-raster-state][raster-offset-depth-bracket]")
{
    for(bool reverse:{false,true})for(bool restore:{false,true})for(F32 units:{-16.f,16.f})
    {
        Convention convention(reverse);Raster raster;
        std::string error;
        auto renderer=ALMetalUIRenderer::createForTexture(*raster.device,*raster.frames,
            ALMetalPixelFormat::rgba8_unorm,&error);REQUIRE(renderer);
        const float z=reverse?.5f:0.f;
        const std::array<std::array<float,4>,3> positions{{{-1,-1,z,1},{3,-1,z,1},{-1,3,z,1}}};
        ALMetalBufferDescriptor desc;desc.length=sizeof(positions);
        auto vertices=ALMetalBuffer::create(*raster.device,desc,&error);REQUIRE(vertices);
        REQUIRE(vertices->write(0,positions.data(),sizeof(positions),&error));
        REQUIRE(renderer->setFrameBackground(raster.scene.getMetalColorTexture(),&error));
        REQUIRE(renderer->setFrameDepth(raster.scene.getMetalDepthTexture(),&error));
        ALMetalUIBatch batch;
        batch.vertices=vertices.get();batch.vertex_count=3;
        batch.shader=ALMetalUIShader::uniform_color;
        batch.blend.enabled=false;
        batch.depth.enabled=true;batch.depth.reverse_z=reverse;
        batch.depth.compare=ALMetalCompareFunction::always;
        batch.depth_bias_units=units;
        batch.target_width=Raster::width;batch.target_height=Raster::height;
        batch.viewport={0,0,Raster::width,Raster::height,0,1};
        batch.scissor={0,0,Raster::width,Raster::height};
        for(U32 i=0;i<4;++i)batch.matrices.modelview_projection[i*5]=1;
        REQUIRE(renderer->draw(batch,&error));
        const ALMetalScissorRect rect{3,2,8,5};
        const F32 far=reverse?0.f:1.f;
        REQUIRE(renderer->clearFrameDepthRects(&rect,1,far,&error));
        if(restore)REQUIRE(renderer->restoreFrameDepthRects(&rect,1,
            reverse?ALMetalCompareFunction::greater:ALMetalCompareFunction::less,&error));
        REQUIRE(renderer->endFrame(&error));REQUIRE(raster.frames->waitUntilIdle(5000,&error));
        REQUIRE(renderer->outputDepthTexture());
        std::vector<F32> actual(Raster::width*Raster::height);
        REQUIRE(renderer->outputDepthTexture()->readback(*raster.device,0,0,{},
            actual.data(),Raster::width*sizeof(F32),&error));
        for(S32 y=0;y<Raster::height;++y)for(S32 x=0;x<Raster::width;++x)
        {
            CAPTURE(reverse,restore,units,x,y);
            const bool inside=x>=3&&x<11&&y>=2&&y<7;
            CHECK(actual[y*Raster::width+x]==(!restore&&inside?far:.5f));
        }
    }
}
