/**
 * @file alhud_diagnostics_metal_test.cpp
 * @brief Actual HUD diagnostic admission with native indexed/line output.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llrendertarget.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include "../../almetal/almetaluirenderer.h"
#include "../../almetal/almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace
{
using Pixel = std::array<U8, 4>;
using Pixels = std::vector<Pixel>;
constexpr U32 side = 64;
namespace Inputs
{
// Geometry metadata only is named here. Actual LLVB, matrices, CPU render state,
// gGL capture, the included production methods and native encoders remain real.
struct LLDrawInfo
{
    LLPointer<LLVertexBuffer> mVertexBuffer;
    U32 mCount = 3, mOffset = 3;
};
struct LLFace
{
    LLVertexBuffer* buffer = nullptr;
    LLVertexBuffer* getVertexBuffer() const { return buffer; }
    U32 getGeomCount() const { return 3; }
    U32 getIndicesCount() const { return 3; }
    S32 getIndicesStart() const { return 3; }
};
struct Capture
{
    bool frame_active = false, world_post_deferred_active = false;
    bool world_post_deferred_phase = false, complete = true;
    U64 render_debug_order = 0;
    std::vector<ALMetalWorldTextureAnimBatch> texture_anim_batches;
    std::vector<ALMetalWorldUpdateTypeBatch> update_type_batches;
    std::vector<ALMetalWorldLightDebugBatch> light_debug_batches;
    void markIncomplete(int) { complete = false; }
};
class LLPipeline
{
public:
    using MetalWorldSimpleCapture = Capture;
    inline static bool sRenderingHUDs = false;
    std::unique_ptr<Capture> mMetalWorldSimpleCapture = std::make_unique<Capture>();
    void captureMetalTextureAnimBatch(const LLDrawInfo&);
    void captureMetalUpdateTypeFace(const LLFace&, const LLColor4&);
    void captureMetalLightDebugBox(const LLVector4a&, const LLVector4a&, const LLColor4&, bool);
};
#include "../../newview/pipeline_hud_debug_native.inl"
}

struct HUD
{
    ALRenderCapabilities old_caps = gRenderCapabilities;
    ALMetalDevice* old_device = LLImageGL::getMetalDevice();
    bool old_authority = LLVertexBuffer::isMetalBufferAuthority();
    bool old_reverse = LLRender::sReverseZ;
    bool old_hud = Inputs::LLPipeline::sRenderingHUDs;
    LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
    U32 old_program = LLGLSLShader::sCurBoundShader;
    LLRender::eMatrixMode old_mode = gGL.getMatrixMode();
    S32 old_viewport[4], old_scissor[4];
    bool old_mask[4];
    LLColor4 old_color = gGL.getDiffuseColor();
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> frames;
    LLGLSLShader debug;
    LLRenderTarget backing;
    LLPointer<LLVertexBuffer> buffer;
    std::string error;
    bool initialized = false, matrices = false, capture_active = false;

    void init(bool reverse = false)
    {
        REQUIRE(old_device == nullptr);
        REQUIRE_FALSE(old_authority);
        REQUIRE(LLVertexBuffer::getBytesAllocated() == 0);
        gGL.getViewport(old_viewport); gGL.getScissor(old_scissor); gGL.getColorMask(old_mask);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        frames = ALMetalFrameScheduler::create(*device, 2, &error); INFO(error); REQUIRE(frames);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLVertexBuffer::initClass(nullptr);
        gGL.setMetalBackend(device.get(), frames.get(), nullptr);
        initialized = true;
        REQUIRE(gGL.initMetal(true));
        LLRender::sReverseZ = reverse;
        for (auto mode : {LLRender::MM_MODELVIEW, LLRender::MM_PROJECTION, LLRender::MM_TEXTURE0})
        { gGL.matrixMode(mode); gGL.pushMatrix(); gGL.loadIdentity(); }
        matrices = true;
        gGL.matrixMode(LLRender::MM_PROJECTION); gGL.ortho(0, side, 0, side, -1, 1);
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        debug.mName = "Native HUD diagnostic source regression";
        debug.mShaderFiles = {{"interface/debugV.glsl", GL_VERTEX_SHADER}, {"interface/debugF.glsl", GL_FRAGMENT_SHADER}};
        debug.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(debug.createShader()); REQUIRE(debug.isAvailable());
        REQUIRE(backing.allocate(side, side, GL_RGBA8, true, false, ALTextureSlot::TT_TEXTURE,
            LLRenderTarget::MIPS_NONE, LLRenderTarget::DEPTH_FMT_32F));
        ALMetalRenderPassConfig config;
        config.colors[0].clear = {0,0,0,.6}; config.clear_depth = .5;
        config.depth_store = ALMetalStoreAction::store;
        auto pass = backing.getMetalRenderTarget()->makeRenderPass(config, &error); REQUIRE(pass);
        auto command = device->makeCommandBuffer("HUD diagnostic authored depth", &error); REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "HUD diagnostic clear", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        buffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(buffer->allocateBuffer(6, 6));
        LLStrider<LLVector3> positions;
        REQUIRE(buffer->getVertexStrider(positions));
        positions[0].set(-90,-90,0); positions[1].set(-80,-90,0); positions[2].set(-90,-80,0);
        positions[3].set(8,8,0); positions[4].set(48,8,0); positions[5].set(8,48,0);
        LLStrider<U16> indices; REQUIRE(buffer->getIndexStrider(indices));
        for (U16 i=0; i<6; ++i) indices[i] = i;
        buffer->unmapBuffer();
    }
    ~HUD()
    {
        if (!initialized) return;
        if (capture_active) gGL.endMetalUITextureCapture(false);
        debug.bind(); gGL.diffuseColor4fv(old_color.mV); debug.unbind();
        buffer = nullptr; backing.release();
        CHECK(frames->waitUntilIdle(5000, &error));
        if (matrices)
            for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION, LLRender::MM_MODELVIEW})
            { gGL.matrixMode(mode); gGL.popMatrix(); }
        gGL.shutdown(); gGL.setMetalBackend(nullptr, nullptr, nullptr);
        LLVertexBuffer::flushBuffers(); CHECK(LLVertexBuffer::getBytesAllocated() == 0);
        LLVertexBuffer::cleanupClass(); LLVertexBuffer::setMetalDevice(old_device);
        LLVertexBuffer::setMetalBufferAuthority(old_authority); LLImageGL::setMetalDevice(old_device);
        gRenderCapabilities = old_caps; LLRender::sReverseZ = old_reverse;
        Inputs::LLPipeline::sRenderingHUDs = old_hud;
        gGL.matrixMode(old_mode); gGL.setViewport(old_viewport[0],old_viewport[1],old_viewport[2],old_viewport[3]);
        gGL.setScissor(old_scissor[0],old_scissor[1],old_scissor[2],old_scissor[3]);
        gGL.setColorMask(old_mask[0],old_mask[1],old_mask[2],old_mask[3]);
        LLGLSLShader::sCurBoundShader = old_program; LLGLSLShader::sCurBoundShaderPtr = old_shader;
    }
    void quad(const LLColor4& color, F32 left=0, F32 bottom=0, F32 right=side, F32 top=side, F32 z=0, bool flush=true)
    {
        gGL.diffuseColor4fv(color.mV);
        gGL.begin(LLRender::TRIANGLES);
        for (auto p : std::array<std::array<F32,2>,6>{{{left,bottom},{right,bottom},{right,top},{left,bottom},{right,top},{left,top}}})
            gGL.vertex3f(p[0],p[1],z);
        gGL.end(); if (flush) gGL.flush();
    }
    void begin()
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
            &gUIProgram, backing.getMetalColorTexture(), backing.getMetalDepthTexture(),
            &debug, LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        capture_active = true;
        gGL.setViewport(0,0,side,side); debug.bind(); gGL.getTextureSlot(0)->unbind();
        gGL.matrixMode(LLRender::MM_MODELVIEW); gGL.loadIdentity();
        {
            LLGLDisable blend(GL_BLEND); LLGLDepthTest depth(GL_FALSE, GL_FALSE);
            gGL.setColorMask(true,true); quad(LLColor4(0,0,0,.6f));
        }
        // These bindings remain in the actual renderDebug/partition owner:
        // RGB-only writes, BT_ALPHA and the current HUD matrix. Per-diagnostic
        // blend is intentionally OFF here to test its removed original scope.
        gGL.setColorMask(true,false); gGL.setSceneBlendType(LLRender::BT_ALPHA);
        Inputs::LLPipeline::sRenderingHUDs = true;
    }
    Pixels finish(bool success=true)
    {
        gGL.flush(); LLRender::EMetalUICaptureEndState state{}; std::string failure;
        auto output = gGL.endMetalUITextureCapture(true,false,nullptr,&failure,&state); capture_active=false; INFO(failure);
        if (!success) { CHECK_FALSE(output); CHECK(state == LLRender::EMetalUICaptureEndState::CAPTURE_INCOMPLETE); return {}; }
        REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE); REQUIRE(output);
        Pixels pixels(side*side); REQUIRE(output->readback(*device,0,0,{},pixels.data(),side*4,&error));
        return pixels;
    }
};
Pixel pixel(const Pixels& p, U32 x, U32 y) { REQUIRE(p.size() == side*side); return p[y*side+x]; }
void alphaIntact(const Pixels& pixels) { for (const auto& p : pixels) CHECK(p[3] == 153); }
}

TEST_CASE("HUD Texture Anim and Update Type diagnostics admit current native face ranges",
          "[almetal][hud-diagnostics][hud-diagnostic-triangles]")
{
    HUD f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE,GL_FALSE);
    Inputs::LLPipeline pipeline; Inputs::LLDrawInfo info; info.mVertexBuffer=f.buffer;
    Inputs::LLFace face{f.buffer.get()};
    const std::array<LLColor4,5> colors{{{0,1,0,.5f},{0,1,1,.5f},{1,0,0,.5f},{1,1,0,.5f},{0,0,1,.5f}}};
    for (const auto& color : colors)
    {
        f.begin(); gGL.translatef(5,7,0);
        pipeline.captureMetalUpdateTypeFace(face,color);
        CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
        const auto actual=f.finish();
        CHECK((pixel(actual,20,22) == Pixel{U8(color.mV[0]*128),U8(color.mV[1]*128),U8(color.mV[2]*128),153}));
        CHECK((pixel(actual,10,10) == Pixel{0,0,0,153})); alphaIntact(actual);
    }
    f.begin(); gGL.translatef(5,7,0);
    f.quad(LLColor4(0,0,1,1), 8,8,24,24,0,false); // Pending original stream.
    pipeline.captureMetalTextureAnimBatch(info);
    CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
    const auto actual=f.finish();
    CHECK((pixel(actual,20,22) == Pixel{128,128,128,153})); alphaIntact(actual);
    CHECK(pipeline.mMetalWorldSimpleCapture->texture_anim_batches.empty());
    CHECK(pipeline.mMetalWorldSimpleCapture->update_type_batches.empty());
}

TEST_CASE("HUD light bounds preserve original disabled versus inherited depth",
          "[almetal][hud-diagnostics][hud-diagnostic-boxes]")
{
    for (bool reverse : {false,true})
    {
        HUD f; f.init(reverse); Inputs::LLPipeline pipeline;
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE,GL_TRUE,GL_LEQUAL);
        LLVector4a center, extent; center.set(.80625f,.80625f,-.5f,1); extent.set(.3f,.3f,.05f,0);
        f.begin(); gGL.scalef(40,40,1); const auto before_color=gGL.getDiffuseColor();
        pipeline.captureMetalLightDebugBox(center,extent,LLColor4::white,false);
        CHECK(LLGLDepthTest::depthEnabled()); CHECK(LLGLDepthTest::depthWriteEnabled());
        CHECK_FALSE(LLGLState::isEnabled(GL_BLEND)); CHECK(gGL.getDiffuseColor()==before_color);
        const auto white=f.finish();
        CHECK(std::any_of(white.begin(),white.end(),[](const Pixel& p){return p[0]==255&&p[1]==255&&p[2]==255;}));
        alphaIntact(white);
        // Independent authored 2-D rectangle: the exact projected box outline,
        // without the producer's corner/edge construction or depth handling.
        f.begin();
        { LLGLDepthTest no_depth(GL_FALSE,GL_FALSE);
          gGL.diffuseColor4fv(LLColor4::white.mV); gGL.begin(LLRender::LINES);
          for (auto p : std::array<std::array<F32,2>,8>{{
              {20.25f,20.25f},{44.25f,20.25f},{44.25f,20.25f},{44.25f,44.25f},
              {44.25f,44.25f},{20.25f,44.25f},{20.25f,44.25f},{20.25f,20.25f}}})
              gGL.vertex3f(p[0],p[1],0);
          gGL.end(); gGL.flush(); }
        const auto rectangle=f.finish(); CHECK(white==rectangle);
        extent.set(.3f,.3f,.3f,0); // The real radius producer supplies a cube.
        f.begin(); gGL.scalef(40,40,1); pipeline.captureMetalLightDebugBox(center,extent,LLColor4::yellow,true);
        const auto far=f.finish();
        for (const auto& p : far) CHECK((p==Pixel{0,0,0,153}));
        center.set(.80625f,.80625f,.5f,1);
        f.begin(); gGL.scalef(40,40,1); pipeline.captureMetalLightDebugBox(center,extent,LLColor4::yellow,true);
        const auto near=f.finish();
        CHECK(std::any_of(near.begin(),near.end(),[](const Pixel& p){return p[0]==255&&p[1]==255&&p[2]==0;}));
        alphaIntact(near);
        // Disabled white bounds must not alter the retained scene depth.
        center.set(.80625f,.80625f,-.5f,1);
        f.begin(); gGL.scalef(40,40,1); pipeline.captureMetalLightDebugBox(center,extent,LLColor4::white,false);
        gGL.loadIdentity();
        { LLGLDepthTest equal(GL_TRUE,GL_FALSE,GL_EQUAL); f.quad(LLColor4::green); }
        const auto equal=f.finish(); for (const auto& p : equal) CHECK((p==Pixel{0,255,0,153}));
    }
}

TEST_CASE("HUD diagnostic admission keeps off world and failed resource paths distinct",
          "[almetal][hud-diagnostics][hud-diagnostic-admission]")
{
    HUD f; f.init(); Inputs::LLPipeline pipeline; Inputs::LLDrawInfo info; info.mVertexBuffer=f.buffer;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE,GL_FALSE);
    f.begin(); Inputs::LLPipeline::sRenderingHUDs=false;
    pipeline.captureMetalTextureAnimBatch(info);
    const auto off=f.finish(); for(const auto& p:off) CHECK((p==Pixel{0,0,0,153}));
    f.begin(); Inputs::LLPipeline::sRenderingHUDs=false;
    auto& world=*pipeline.mMetalWorldSimpleCapture; world.frame_active=true; world.world_post_deferred_active=true;
    pipeline.captureMetalTextureAnimBatch(info);
    Inputs::LLFace face{f.buffer.get()}; pipeline.captureMetalUpdateTypeFace(face,LLColor4(0,1,0,.5f));
    LLVector4a center,extent; center.set(1,2,3,1); extent.set(4,5,6,0);
    pipeline.captureMetalLightDebugBox(center,extent,LLColor4::white,false);
    REQUIRE(world.texture_anim_batches.size()==1); REQUIRE(world.update_type_batches.size()==1);
    REQUIRE(world.light_debug_batches.size()==1); CHECK(world.render_debug_order==3); CHECK(world.complete);
    CHECK(world.texture_anim_batches[0].index_offset==6); CHECK(world.update_type_batches[0].index_offset==6);
    const auto only_world=f.finish(); CHECK(only_world==off);
    world.frame_active=false; world.world_post_deferred_active=false;
    f.begin();
    struct MissingPublisher { ALMetalDevice* device; MissingPublisher(ALMetalDevice* d):device(d){LLVertexBuffer::setMetalDevice(nullptr);} ~MissingPublisher(){LLVertexBuffer::setMetalDevice(device);} };
    {
        MissingPublisher missing(f.device.get()); LLStrider<LLVector3> positions;
        REQUIRE(f.buffer->getVertexStrider(positions)); positions[4].set(40,8,0);
        f.buffer->unmapBuffer(); const U64 stamp=f.buffer->getContentStamp();
        const auto* old=f.buffer->getMetalVertexBuffer();
        pipeline.captureMetalTextureAnimBatch(info);
        CHECK_FALSE(f.buffer->hasPublishedMetalVertexBuffer());
        CHECK(f.buffer->getMetalVertexBuffer()==old); CHECK(f.buffer->getContentStamp()==stamp);
        f.finish(false);
    }
    f.begin(); pipeline.captureMetalTextureAnimBatch(info); const auto retried=f.finish();
    CHECK(f.buffer->hasPublishedMetalVertexBuffer());
    CHECK((pixel(retried,20,20)==Pixel{128,128,0,153}));
}
