/**
 * @file alhud_selection_metal_test.cpp
 * @brief Actual selected-face admission reaches the ordered native HUD owner.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include "../llglstates.h"
#include "../../almetal/almetalpipeline.h"
#include "../../almetal/almetaluirenderer.h"
#include "../../almetal/almetalworld.h"
#include <array>
#include <cmath>
#include <cstring>

namespace
{
using Pixel = std::array<U8, 4>;
constexpr U32 width = 16, height = 12;
constexpr Pixel background{20, 40, 60, 200};
constexpr std::array<Pixel, 4> checker{{
    {255, 0, 0, 255}, {0, 255, 0, 128},
    {0, 0, 255, 64}, {255, 255, 255, 192}}};

// Only viewer ownership/input wrappers are substituted. The included methods
// are the production bodies, using real LLVB stores, LLImageGL, gGL, native
// command encoders and textures. The selected list is already TE-filtered:
// this fixture does not instantiate LLSelectMgr, LLFace or a viewer scene.
struct SelectionImageInput
{
    LLPointer<LLImageGL> image;
    LLImageGL* getGLTexture() const { return image; }
    static SelectionImageInput* sNullImagep;
};
SelectionImageInput* SelectionImageInput::sNullImagep = nullptr;
struct SelectionFaceInput;
struct SelectionPipelineInput
{
    enum : U32 { RENDER_DEBUG_FEATURE_SELECTED = 2 };
    struct MetalWorldSimpleCapture
    {
        bool frame_active = false, world_post_deferred_active = false;
        bool world_post_deferred_phase = false, complete = true;
        std::vector<ALMetalWorldSimpleBatch> batches;
        std::vector<ALMetalWorldHighlightBatch> highlight_batches;
        void markIncomplete(U32) { complete = false; }
    };
    std::unique_ptr<MetalWorldSimpleCapture> mMetalWorldSimpleCapture =
        std::make_unique<MetalWorldSimpleCapture>();
    static bool sRenderingHUDs;
    static LLRender::eTexIndex sRenderHighlightTextureChannel;
    U32 feature_mask = RENDER_DEBUG_FEATURE_SELECTED; // UI intentionally off, as in the HUD bracket.
    std::vector<SelectionFaceInput*> selected, mHighlightFaces;
    SelectionImageInput* selection_image = nullptr;
    U32 initialized_checks = 0;
    void assertInitialized() { ++initialized_checks; }
    bool hasRenderDebugFeatureMask(U32 bit) const { return (feature_mask & bit) != 0; }
    void renderSelectedFaces(const LLColor4&, U32);
    void renderHighlights();
    void captureMetalWorldHighlightBatch(LLVertexBuffer*, U32, U32,
        SelectionImageInput*, const LLColor4&, U32);
};
bool SelectionPipelineInput::sRenderingHUDs = false;
LLRender::eTexIndex SelectionPipelineInput::sRenderHighlightTextureChannel = LLRender::DIFFUSE_MAP;
struct SelectionFaceInput
{
    SelectionPipelineInput* pipeline = nullptr;
    LLPointer<LLVertexBuffer> buffer;
    U32 index_start = 3, index_count = 6;
    void renderSelected(SelectionImageInput* image, const LLColor4& color, U32 uv)
    { pipeline->captureMetalWorldHighlightBatch(buffer, index_count, index_start, image, color, uv); }
};
void SelectionPipelineInput::renderSelectedFaces(const LLColor4& color, U32 uv)
{ for (auto* face : selected) face->renderSelected(selection_image, color, uv); }

#pragma push_macro("LLPipeline")
#pragma push_macro("LLViewerTexture")
#define LLPipeline SelectionPipelineInput
#define LLViewerTexture SelectionImageInput
#include "../../newview/pipeline_highlight_native.inl"
#pragma pop_macro("LLViewerTexture")
#pragma pop_macro("LLPipeline")

struct Selection : ll_test::NativeFontUI
{
    bool old_reverse = LLRender::sReverseZ;
    bool old_hud = SelectionPipelineInput::sRenderingHUDs;
    LLRender::eTexIndex old_channel = SelectionPipelineInput::sRenderHighlightTextureChannel;
    SelectionPipelineInput pipeline;
    std::unique_ptr<ALMetalRenderTarget> backing;
    SelectionImageInput image, white;
    std::string error;
    explicit Selection(bool reverse = false)
    {
        LLRender::sReverseZ = reverse;
        initRenderer();
        SelectionPipelineInput::sRenderingHUDs = true;
        SelectionPipelineInput::sRenderHighlightTextureChannel = LLRender::DIFFUSE_MAP;
        gGL.matrixMode(LLRender::MM_PROJECTION);gGL.loadIdentity();
        gGL.ortho(0, width, 0, height, -1, 1);
        gGL.matrixMode(LLRender::MM_MODELVIEW);gGL.loadIdentity();
        gGL.setViewport(0, 0, width, height);
        ALMetalRenderTargetDescriptor desc;
        desc.width = width;desc.height = height;
        desc.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        desc.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        desc.depth_stencil_shader_read = true;
        backing = ALMetalRenderTarget::create(*device, desc, &error);REQUIRE(backing);
        ALMetalRenderPassConfig config;
        config.colors[0].clear = {20./255.,40./255.,60./255.,200./255.};
        config.clear_depth = .5;config.depth_store = ALMetalStoreAction::store;
        auto pass = backing->makeRenderPass(config, &error);REQUIRE(pass);
        auto command = device->makeCommandBuffer("HUD selection authored backing", &error);REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "HUD selection clear", &error);REQUIRE(encoder);
        REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));
        image.image = makeImage(checker);
        white.image = makeImage({Pixel{255,255,255,255},Pixel{255,255,255,255},
                                Pixel{255,255,255,255},Pixel{255,255,255,255}});
        SelectionImageInput::sNullImagep = &white;
        pipeline.selection_image = &image;
    }
    ~Selection()
    {
        if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
        SelectionImageInput::sNullImagep = nullptr;
        SelectionPipelineInput::sRenderingHUDs = old_hud;
        SelectionPipelineInput::sRenderHighlightTextureChannel = old_channel;
        LLRender::sReverseZ = old_reverse;
    }
    LLPointer<LLImageGL> makeImage(const std::array<Pixel,4>& pixels)
    {
        LLPointer<LLImageRaw> raw = new LLImageRaw(2,2,4);
        std::memcpy(raw->getData(),pixels.data(),sizeof(pixels));
        LLPointer<LLImageGL> result = new LLImageGL(raw,false);
        REQUIRE(result->getMetalTexture());return result;
    }
    SelectionFaceInput face(U32 cell = 0, bool wide = false, F32 z = .2f)
    {
        SelectionFaceInput face;face.pipeline = &pipeline;
        face.buffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1 | LLVertexBuffer::MAP_TEXCOORD2);
        REQUIRE(face.buffer->allocateBuffer(4,wide?18:9));
        std::array<LLVector4a,4> positions;
        const F32 x = 1 + (cell%2)*4, y = 1 + (cell/2)*4;
        positions[0].set(x,y,z,1);positions[1].set(x+3,y,z,1);
        positions[2].set(x+3,y+3,z,1);positions[3].set(x,y+3,z,1);
        face.buffer->setPositionData(positions.data());
        for (U32 channel=0;channel<3;++channel)
        {
            LLStrider<LLVector2> uv;
            if(channel==0) REQUIRE(face.buffer->getTexCoord0Strider(uv));
            if(channel==1) REQUIRE(face.buffer->getTexCoord1Strider(uv));
            if(channel==2) REQUIRE(face.buffer->getTexCoord2Strider(uv));
            const U32 sample = cell ^ channel;
            for(U32 i=0;i<4;++i) uv[i].set(.25f+.5f*(sample%2),.25f+.5f*(sample/2));
        }
        if(wide) { const U32 indices[]{0,0,0,0,1,2,0,2,3};face.buffer->setIndexData(indices); }
        else { const U16 indices[]{0,0,0,0,1,2,0,2,3};face.buffer->setIndexData(indices); }
        face.buffer->unmapBuffer();
        REQUIRE(face.buffer->hasPublishedMetalVertexBuffer());
        REQUIRE(face.buffer->hasPublishedMetalIndexBuffer());return face;
    }
    void immediate(Pixel pixel, U32 left=0,U32 bottom=0,U32 right=width,U32 top=height, bool flush=true)
    {
        gUIProgram.bind();gGL.getTextureSlot(0)->unbind();
        gGL.color4ub(pixel[0],pixel[1],pixel[2],pixel[3]);
        gGL.begin(LLRender::TRIANGLES);
        gGL.vertex3f(left,bottom,.2f);gGL.vertex3f(right,bottom,.2f);gGL.vertex3f(right,top,.2f);
        gGL.vertex3f(left,bottom,.2f);gGL.vertex3f(right,top,.2f);gGL.vertex3f(left,top,.2f);
        gGL.end();if(flush)gGL.flush();
    }
    void begin(bool seed = true)
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,&gUIProgram,
            backing->colorTexture(0),backing->depthStencilTexture(),nullptr,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setColorMask(true,true);
        gGL.setSceneBlendType(LLRender::BT_REPLACE);
        if(seed) immediate(background);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true,false); // Original renderGeomPostDeferred boundary.
    }
    std::unique_ptr<ALMetalTexture> finish(bool complete = true, bool cancel = false)
    {
        gGL.flush();LLRender::EMetalUICaptureEndState state{};
        auto output = gGL.endMetalUITextureCapture(!cancel,false,nullptr,&error,&state);
        INFO(error);
        if(complete&&!cancel) { REQUIRE(state==LLRender::EMetalUICaptureEndState::COMPLETE);REQUIRE(output); }
        else { CHECK_FALSE(output);if(!cancel) CHECK(state==LLRender::EMetalUICaptureEndState::CAPTURE_INCOMPLETE); }
        return output;
    }
    std::vector<Pixel> read(const ALMetalTexture& texture)
    {
        std::vector<Pixel> pixels(width*height);
        REQUIRE(texture.readback(*device,0,0,{},pixels.data(),width*4,&error));return pixels;
    }
    static Pixel blend(Pixel old, Pixel texture, const LLColor4& color, bool alpha=false)
    {
        const float a = texture[3]/255.f*color.mV[3];Pixel result = old;
        for(U32 i=0;i<(alpha?4u:3u);++i)
        {
            const float source = texture[i]*color.mV[i];
            result[i] = static_cast<U8>(std::lround(source*a+old[i]*(1-a)));
        }
        return result;
    }
    static void checkPixel(Pixel actual, Pixel expected)
    { for(U32 c=0;c<4;++c) { CAPTURE(c,int(actual[c]),int(expected[c]));CHECK(std::abs(int(actual[c])-int(expected[c]))<=1); } }
};
}

TEST_CASE("Actual selected HUD admission honors the feature and selected texture channel",
          "[almetal][native-hud-selection][native-hud-selection-blend]")
{
    for(bool reverse:{false,true})for(U32 channel=0;channel<3;++channel)for(bool enabled:{false,true})
    {
        Selection f(reverse);LLGLDisable blend(GL_BLEND),cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LEQUAL);
        std::array<SelectionFaceInput,4> faces;
        for(U32 i=0;i<4;++i) { faces[i]=f.face(i,i%2);f.pipeline.selected.push_back(&faces[i]); }
        SelectionPipelineInput::sRenderHighlightTextureChannel = channel==0?LLRender::DIFFUSE_MAP:
            channel==1?LLRender::NORMAL_MAP:LLRender::SPECULAR_MAP;
        f.pipeline.feature_mask = enabled?SelectionPipelineInput::RENDER_DEBUG_FEATURE_SELECTED:0;
        f.begin();gGL.translatef(2,1,0);f.pipeline.renderHighlights();
        CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
        const auto pixels=f.read(*f.finish());
        const LLColor4 tint = channel==0?LLColor4(1,1,1,.5f):channel==1?LLColor4(1,.5f,.5f,.5f):LLColor4(0,.3f,1,.8f);
        for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)
        {
            Pixel expected=background;
            for(U32 i=0;i<4;++i)if(enabled && x>=3+(i%2)*4 && x<6+(i%2)*4 && y>=2+(i/2)*4 && y<5+(i/2)*4)
                expected=Selection::blend(background,checker[i^channel],tint);
            CAPTURE(reverse,channel,enabled,x,y);Selection::checkPixel(pixels[y*width+x],expected);
        }
        CHECK(f.pipeline.initialized_checks==1);CHECK(f.pipeline.mHighlightFaces.empty());
        CHECK(f.pipeline.mMetalWorldSimpleCapture->highlight_batches.empty());
    }
}

TEST_CASE("Selected HUD checker preserves texture transforms, alpha mask and preceding immediate order",
          "[almetal][native-hud-selection]")
{
    Selection f;LLGLEnable blend(GL_BLEND);LLGLDisable cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LEQUAL);
    auto face=f.face();f.begin();
    // -.25 wraps to .75 with the original AnisoWrap sampler; ignoring the
    // matrix or clamping the coordinate instead would select the red cell.
    gGL.matrixMode(LLRender::MM_TEXTURE0);gGL.translatef(-.5f,0,0);
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    const Pixel earlier{100,80,60,200};
    f.immediate(earlier,1,1,4,4,false); // Deliberately queued under identical state.
    const LLColor4 tint(.25f,.75f,.5f,.5f);
    face.renderSelected(&f.image,tint,LLVertexBuffer::TYPE_TEXCOORD0);
    const auto pixels=f.read(*f.finish());
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)
    {
        const Pixel expected=x>=1&&x<4&&y>=1&&y<4?
            Selection::blend(Selection::blend(background,earlier,LLColor4(1,1,1,1)),checker[1],tint):background;
        CAPTURE(x,y);Selection::checkPixel(pixels[y*width+x],expected);
    }
    f.begin();gGL.setSceneBlendType(LLRender::BT_REPLACE);gGL.setColorMask(true,true);
    face.renderSelected(&f.image,tint,LLVertexBuffer::TYPE_TEXCOORD0);
    const auto alpha=f.read(*f.finish());
    for(U32 c=0;c<4;++c) CHECK(std::abs(int(alpha[2*width+2][c])-int(std::lround(checker[1][c]*tint.mV[c])))<=1);
}

TEST_CASE("Selected HUD batches retain depth, red highlight ordering and command-owned resources",
          "[almetal][native-hud-selection]")
{
    for(bool reverse:{false,true})
    {
        Selection f(reverse);LLGLEnable blend(GL_BLEND);LLGLDisable cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LEQUAL);
        auto visible=f.face(0),hidden=f.face(1,false,-.2f);
        f.pipeline.selected={&visible,&hidden};f.pipeline.mHighlightFaces={&visible};
        f.begin();f.pipeline.renderHighlights();
        visible.buffer=nullptr;hidden.buffer=nullptr;f.image.image=nullptr;f.white.image=nullptr;
        f.pipeline.selected.clear();LLVertexBuffer::unbind();
        const auto pixels=f.read(*f.finish());
        const auto selected=Selection::blend(background,checker[0],LLColor4(1,1,1,.5f));
        const auto red=Selection::blend(selected,Pixel{255,255,255,255},LLColor4(1,0,0,.5f));
        for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)
        { CAPTURE(reverse,x,y);Selection::checkPixel(pixels[y*width+x],x>=1&&x<4&&y>=1&&y<4?red:background); }
        CHECK(f.pipeline.mHighlightFaces.empty());
    }
}

TEST_CASE("Invalid selected HUD batches fail the complete capture closed and allow retry",
          "[almetal][native-hud-selection]")
{
    Selection f;LLGLEnable blend(GL_BLEND);LLGLDisable cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LEQUAL);
    auto face=f.face();f.begin();face.renderSelected(&f.image,LLColor4(1,1,1,.5f),LLVertexBuffer::TYPE_TEXCOORD0);
    auto committed=f.finish();const auto old=f.read(*committed);
    for(U32 invalid=0;invalid<4;++invalid)
    {
        f.begin();
        f.pipeline.captureMetalWorldHighlightBatch(invalid==0?nullptr:face.buffer.get(),6,
            invalid==1?999:3,invalid==2?nullptr:&f.image,LLColor4(1,1,1,.5f),
            invalid==3?LLVertexBuffer::TYPE_TEXCOORD3:LLVertexBuffer::TYPE_TEXCOORD0);
        f.finish(false);CHECK(f.read(*committed)==old);
    }
    f.begin();face.renderSelected(&f.image,LLColor4(1,1,1,.5f),LLVertexBuffer::TYPE_TEXCOORD0);f.finish(false,true);
    f.begin();face.renderSelected(&f.image,LLColor4(1,1,1,.5f),LLVertexBuffer::TYPE_TEXCOORD0);
    CHECK(f.read(*f.finish())==old);
}

TEST_CASE("Selected face admission preserves world batches, empty draws and lazy native publication",
          "[almetal][native-hud-selection]")
{
    Selection f;LLGLEnable blend(GL_BLEND);LLGLDisable cull(GL_CULL_FACE),scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LEQUAL);
    auto face=f.face();
    // There is no active world or UI owner: never enqueue a future-frame draw.
    const auto submissions=ALMetalDevice::committedCommandBufferCount();
    face.renderSelected(&f.image,LLColor4(1,1,1,.5f),LLVertexBuffer::TYPE_TEXCOORD0);
    CHECK(ALMetalDevice::committedCommandBufferCount()==submissions);
    CHECK(f.pipeline.mMetalWorldSimpleCapture->highlight_batches.empty());
    SelectionPipelineInput::sRenderingHUDs=false;
    auto& world=*f.pipeline.mMetalWorldSimpleCapture;
    world.frame_active=world.world_post_deferred_active=true;
    const LLColor4 first(.2f,.4f,.6f,.8f),second(.8f,.6f,.4f,.2f);
    gGL.setColorMask(true,false); // Actual original post-deferred caller.
    face.renderSelected(&f.image,first,LLVertexBuffer::TYPE_TEXCOORD1);
    gGL.setColorMask(true,true);  // Generic caller can still request RGBA.
    face.renderSelected(&f.image,second,LLVertexBuffer::TYPE_TEXCOORD2);
    REQUIRE(world.highlight_batches.size()==2);
    CHECK(world.highlight_batches[0].texcoord_offset==face.buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD1));
    CHECK(world.highlight_batches[1].texcoord_offset==face.buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD2));
    CHECK(world.highlight_batches[0].index_offset==3*sizeof(U16));
    CHECK(world.highlight_batches[0].index_count==6);
    CHECK(world.highlight_batches[0].texture==f.image.image->getMetalTexture());
    CHECK_FALSE(world.highlight_batches[0].write_alpha);
    CHECK(world.highlight_batches[1].write_alpha);
    for(U32 c=0;c<4;++c)
    { CHECK(world.highlight_batches[0].color[c]==first.mV[c]);CHECK(world.highlight_batches[1].color[c]==second.mV[c]); }
    CHECK(world.complete);
    CHECK(ALMetalDevice::committedCommandBufferCount()==submissions);
    face.renderSelected(&f.image,first,LLVertexBuffer::TYPE_TEXCOORD3);
    CHECK_FALSE(world.complete);CHECK(world.highlight_batches.empty());
    world.frame_active=world.world_post_deferred_active=false;
    SelectionPipelineInput::sRenderingHUDs=true;
    f.begin();
    f.pipeline.captureMetalWorldHighlightBatch(nullptr,0,0,nullptr,first,LLVertexBuffer::TYPE_TEXCOORD0);
    const auto empty=f.read(*f.finish());for(auto p:empty) CHECK(p==background);
    // The original setBuffer boundary publishes new CPU bytes even when the
    // previous native allocation still exists. No global flushBuffers runs.
    LLStrider<LLVector2> uv;REQUIRE(face.buffer->getTexCoord0Strider(uv));
    for(U32 i=0;i<4;++i) uv[i].set(.75f,.75f);
    REQUIRE_FALSE(face.buffer->hasPublishedMetalVertexBuffer());
    REQUIRE(face.buffer->getMetalVertexBuffer());
    f.begin();face.renderSelected(&f.image,LLColor4(1,1,1,.5f),LLVertexBuffer::TYPE_TEXCOORD0);
    CHECK(face.buffer->hasPublishedMetalVertexBuffer());
    const auto fresh=f.read(*f.finish());
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)
    {
        const auto expected=x>=1&&x<4&&y>=1&&y<4?
            Selection::blend(background,checker[3],LLColor4(1,1,1,.5f)):background;
        CAPTURE(x,y);Selection::checkPixel(fresh[y*width+x],expected);
    }
}

TEST_CASE("Actual world selected-face recording preserves the caller's glow alpha mask",
          "[almetal][native-hud-selection][native-world-selection-mask]")
{
    for(bool reverse:{false,true})for(bool hdr:{false,true})
    {
        Selection f(reverse);auto face=f.face();
        SelectionPipelineInput::sRenderingHUDs=false;
        auto& world=*f.pipeline.mMetalWorldSimpleCapture;
        world.frame_active=world.world_post_deferred_active=true;
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        const auto target=[&](bool gbuffer)
        {
            ALMetalRenderTargetDescriptor desc;
            desc.width=width;desc.height=height;
            if(gbuffer)
            {
                desc.color_attachment_count=4;
                desc.colors[0].format=ALMetalPixelFormat::rgba8_unorm_srgb;
                desc.colors[1].format=ALMetalPixelFormat::rgba8_unorm;
                desc.colors[2].format=ALMetalPixelFormat::rgba16_unorm;
                desc.colors[3].format=hdr?ALMetalPixelFormat::rg11b10_float:ALMetalPixelFormat::rgba8_unorm;
                desc.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
                desc.depth_stencil_shader_read=true;
            }
            else desc.colors[0].format=ALMetalPixelFormat::rgba16_float;
            auto result=ALMetalRenderTarget::create(*f.device,desc,&f.error);REQUIRE(result);
            ALMetalRenderPassConfig config;config.colors[0].clear={0,0,0,.75};
            config.clear_depth=.5;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
            auto pass=result->makeRenderPass(config,&f.error);REQUIRE(pass);
            auto command=f.device->makeCommandBuffer("World selected alpha authored backing",&f.error);REQUIRE(command);
            auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"World selected alpha clear",&f.error);REQUIRE(encoder);
            REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error));return result;
        };
        auto gbuffer=target(true),composed=target(false);
        for(bool write_alpha:{false,true})
        {
            world.highlight_batches.clear();gGL.setColorMask(true,write_alpha);
            face.renderSelected(&f.image,LLColor4(1,1,1,.5f),LLVertexBuffer::TYPE_TEXCOORD0);
            REQUIRE(world.highlight_batches.size()==1);
            CHECK(world.highlight_batches[0].write_alpha==write_alpha);
            ALMetalWorldOpaquePostDeferredScene scene;
            scene.width=width;scene.height=height;scene.hdr=hdr;scene.reverse_z=reverse;
            scene.gbuffer=gbuffer.get();scene.composed_lighting=composed.get();
            scene.highlight_batches=world.highlight_batches.data();scene.highlight_batch_count=1;
            auto output=renderer->renderOpaquePostDeferred(scene,&f.error);INFO(f.error);REQUIRE(output);
            std::array<std::array<U16,4>,width*height> pixels;
            REQUIRE(output->colorTexture(0)->readback(*f.device,0,0,{},pixels.data(),width*8,&f.error));
            U32 colored=0,changed_alpha=0;
            for(const auto& pixel:pixels)
            {
                if(pixel[0]) { ++colored;CHECK(pixel[0]==0x3800); }
                CHECK(pixel[1]==0);CHECK(pixel[2]==0);
                // GL's SOURCE_ALPHA / ONE_MINUS_SOURCE_ALPHA also blends
                // alpha: .5*.5 + .75*(1-.5) = .625, exactly half0x3900.
                if(pixel[3]!=0x3a00) { ++changed_alpha;CHECK(write_alpha);CHECK(pixel[3]==0x3900); }
            }
            CHECK(colored==9);CHECK(changed_alpha==(write_alpha?9:0));
            std::array<F32,width*height> actual_depth;
            REQUIRE(output->depthStencilTexture()->readback(*f.device,0,0,{},actual_depth.data(),width*sizeof(F32),&f.error));
            for(F32 value:actual_depth) CHECK(value==.5f);
        }
    }
}
