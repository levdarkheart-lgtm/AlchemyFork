/**
 * @file alpipeline_buffer_publication_metal_test.cpp
 * @brief Actual native capture owners prepare current LLVB stores before copying handles.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include "../../almetal/almetalpipeline.h"
#include "../../almetal/almetalworld.h"
#include <array>
#include <cstring>

namespace
{
namespace Inputs
{
// Explicit viewer frame/material inputs only. The included capture bodies,
// LLVB publisher and typed Metal resources/commands/renderers are real.
struct LLViewerTexture { LLImageGL* image=nullptr; LLImageGL* getGLTexture() const{return image;} };
template<class T> struct Ref { T* value=nullptr; T* get() const{return value;} };
struct LLViewerFetchedTexture : LLViewerTexture
{
    inline static Ref<LLViewerFetchedTexture> sWhiteImagep,sDefaultImagep;
};
struct LLVOAvatar
{
    struct MatrixPaletteCache { std::vector<int> mMatrixPalette;std::vector<F32> mGLMp; };
    const MatrixPaletteCache& updateSkinInfoMatrixPalette(void*) { static MatrixPaletteCache unused;return unused; }
};
struct LLDrawInfo
{
    LLPointer<LLVertexBuffer> mVertexBuffer;
    U32 mCount=3,mOffset=0;
    std::vector<Ref<LLViewerTexture>> mTextureList;
    Ref<LLViewerTexture> mTexture;
    LLMatrix4* mTextureMatrix=nullptr;
    LLVOAvatar* mAvatar=nullptr;void* mSkinInfo=nullptr;
    F32 mAlphaMaskCutoff=.5f;
};
struct Capture
{
    bool frame_active=true,pass_active=true,complete=true;
    bool legacy_glow=false,water_exclusion=false;
    bool impostor_fullbright=false,impostor_shiny=false,impostor_emboss=false;
    bool alpha_mask=false,rigged=false;
    bool windlight_stars_active=true,windlight_stars_complete=true;
    std::vector<ALMetalWorldSimpleBatch> batches;
    ALMetalWorldStarBatch windlight_star_batch;
    void markIncomplete(U32) {complete=false;}
};
struct LLPipeline
{
    using MetalWorldSimpleCapture=Capture;
    std::unique_ptr<Capture> mMetalWorldSimpleCapture=std::make_unique<Capture>();
    struct {bool isMirrorPass() const{return false;}} mHeroProbeManager;
    void captureMetalWorldGBufferBatch(const LLDrawInfo&,bool);
    void captureMetalWorldWindLightStarBatch(LLVertexBuffer*,const LLMatrix4a&);
};
bool gCubeSnapshot=false;
// Named fixed-camera inputs. Production's full matrix helpers remain unchanged;
// this fixture tests publication and captured store identity, not camera policy.
void buildMetalWorldDrawMatrices(const LLDrawInfo&,LLMatrix4a& view,LLMatrix4a& mvp)
{view=LLMatrix4a::identity();mvp=LLMatrix4a::identity();}
bool buildMetalWorldDrawMatrices(const LLDrawInfo& info,LLMatrix4a& view,LLMatrix4a& inverse,LLMatrix4a& mvp)
{buildMetalWorldDrawMatrices(info,view,mvp);inverse=LLMatrix4a::identity();return true;}
#include "../../newview/pipeline_buffer_publication_native.inl"
}

struct MissingPublisher
{
    ALMetalDevice* device;
    explicit MissingPublisher(ALMetalDevice* d):device(d){LLVertexBuffer::setMetalDevice(nullptr);}
    ~MissingPublisher(){LLVertexBuffer::setMetalDevice(device);}
};
struct Publication : ll_test::NativeFontUI
{
    static constexpr U32 side=16;
    LLPointer<LLImageGL> white;
    Inputs::LLViewerTexture image;
    std::string error;
    Publication()
    {
        initRenderer();
        LLPointer<LLImageRaw> raw=new LLImageRaw(1,1,4);
        std::memset(raw->getData(),255,4);white=new LLImageGL(raw,false);
        REQUIRE(white->getMetalTexture());image.image=white;
    }
    LLPointer<LLVertexBuffer> buffer(bool require_published=true)
    {
        LLPointer<LLVertexBuffer> result=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR);
        REQUIRE(result->allocateBuffer(3,3));
        std::memset(result->getMappedData(),0,result->getSize()); // position.w selects texture slot zero.
        positions(*result,false);
        LLStrider<LLVector3> normal;REQUIRE(result->getNormalStrider(normal));
        LLStrider<LLVector2> uv;REQUIRE(result->getTexCoord0Strider(uv));
        LLStrider<LLColor4U> color;REQUIRE(result->getColorStrider(color));
        for(U32 i=0;i<3;++i){normal[i].set(0,0,1);uv[i].set(.5f,.5f);color[i].set(255,0,0,255);}
        const U16 indices[]{0,1,2};result->setIndexData(indices);result->unmapBuffer();
        if(require_published)
        { REQUIRE(result->hasPublishedMetalVertexBuffer());REQUIRE(result->hasPublishedMetalIndexBuffer()); }
        return result;
    }
    void positions(LLVertexBuffer& buffer,bool right)
    {
        LLStrider<LLVector3> p;REQUIRE(buffer.getVertexStrider(p));
        p[0].set(right?.1f:-1.f,-1,0);p[1].set(right?1.f:-.1f,-1,0);p[2].set(right?.1f:-1.f,1,0);
    }
    template<class T> std::vector<T> copied(const ALMetalBuffer& source,std::size_t offset,std::size_t count)
    {
        ALMetalBufferDescriptor d;d.length=count*sizeof(T);d.label="Actual captured store verification";
        auto copy=ALMetalBuffer::create(*device,d,&error);REQUIRE(copy);
        auto command=device->makeCommandBuffer("Actual captured buffer copy",&error);REQUIRE(command);
        REQUIRE(copy->copyFrom(*command,source,offset,0,d.length,&error));REQUIRE(command->commitAndWait(&error));
        std::vector<T> result(count);REQUIRE(copy->read(0,result.data(),d.length,&error));return result;
    }
    std::vector<std::array<U8,4>> render(const ALMetalWorldSimpleBatch& batch)
    {
        auto renderer=ALMetalWorldGBufferRenderer::create(*device,&error);INFO(error);REQUIRE(renderer);
        ALMetalWorldGBufferScene scene;scene.batches=&batch;scene.batch_count=1;scene.width=scene.height=side;
        std::fill(std::begin(scene.clear_color),std::end(scene.clear_color),0.f);
        auto output=renderer->render(scene,&error);INFO(error);REQUIRE(output);
        std::vector<std::array<U8,4>> pixels(side*side);
        REQUIRE(output->colorTexture(0)->readback(*device,0,0,{},pixels.data(),side*4,&error));return pixels;
    }
};
}

TEST_CASE("Actual opaque capture prepares before copying a handle that later setBuffer replaces",
          "[almetal][pipeline-buffer-publication]")
{
    Publication f;Inputs::LLPipeline pipeline;Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    const auto* old=info.mVertexBuffer->getMetalVertexBuffer();
    f.positions(*info.mVertexBuffer,true);REQUIRE_FALSE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
    pipeline.captureMetalWorldGBufferBatch(info,false);
    auto& capture=*pipeline.mMetalWorldSimpleCapture;REQUIRE(capture.complete);REQUIRE(capture.batches.size()==1);
    // This is the original draw's later retry. The unfixed capture already
    // copied old here; it cannot repair that record by publishing afterward.
    info.mVertexBuffer->setBuffer();REQUIRE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
    CHECK(info.mVertexBuffer->getMetalVertexBuffer()!=old);
    CHECK(capture.batches[0].vertices==info.mVertexBuffer->getMetalVertexBuffer());
    CHECK(f.copied<F32>(*capture.batches[0].vertices,capture.batches[0].position_offset,4)[0]==.1f);
    const auto pixels=f.render(capture.batches[0]);
    CHECK(pixels[4*Publication::side+2][0]==0);CHECK(pixels[4*Publication::side+10][0]==255);
}

TEST_CASE("Actual indexed capture rejects warm failed stores and retries only its own producer",
          "[almetal][pipeline-buffer-publication]")
{
    Publication f;
    for(bool water:{false,true})for(bool vertex:{false,true})
    {
        Inputs::LLPipeline pipeline;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
        auto& capture=*pipeline.mMetalWorldSimpleCapture;capture.water_exclusion=water;
        auto unrelated=f.buffer();f.positions(*unrelated,true);
        {
            MissingPublisher missing(f.device.get());
            if(vertex) f.positions(*info.mVertexBuffer,true);
            else {const U16 indices[]{2,1,0};info.mVertexBuffer->setIndexData(indices);}
            LLVertexBuffer::flushBuffers();LLVertexBuffer::flushBuffers();
            REQUIRE(info.mVertexBuffer->getMetalVertexBuffer());REQUIRE(info.mVertexBuffer->getMetalIndexBuffer());
            CHECK_FALSE((vertex?info.mVertexBuffer->hasPublishedMetalVertexBuffer():info.mVertexBuffer->hasPublishedMetalIndexBuffer()));
            pipeline.captureMetalWorldGBufferBatch(info,false);
            CHECK_FALSE(capture.complete);CHECK(capture.batches.empty());
        }
        capture.complete=true;capture.batches.clear();
        pipeline.captureMetalWorldGBufferBatch(info,false);
        CHECK(capture.complete);REQUIRE(capture.batches.size()==1);
        CHECK(capture.batches[0].vertices==info.mVertexBuffer->getMetalVertexBuffer());
        CHECK(capture.batches[0].indices==info.mVertexBuffer->getMetalIndexBuffer());
        CHECK(info.mVertexBuffer->hasPublishedMetalVertexBuffer());CHECK(info.mVertexBuffer->hasPublishedMetalIndexBuffer());
        CHECK_FALSE(unrelated->hasPublishedMetalVertexBuffer()); // No global dirty-buffer veto or global flush.
        CHECK(capture.batches[0].water_exclusion==water);
        if(vertex) CHECK(f.copied<F32>(*capture.batches[0].vertices,0,4)[0]==.1f);
        else CHECK((f.copied<U32>(*capture.batches[0].indices,0,1)[0]&0xffffu)==2);
    }
}

TEST_CASE("Actual vertex-only star capture ignores an unpublished unused index store",
          "[almetal][pipeline-buffer-publication]")
{
    Publication f;Inputs::LLPipeline pipeline;auto buffer=f.buffer();
    const auto mvp=LLMatrix4a::identity();const auto* old=buffer->getMetalVertexBuffer();
    f.positions(*buffer,true);
    pipeline.captureMetalWorldWindLightStarBatch(buffer,mvp);
    auto& capture=*pipeline.mMetalWorldSimpleCapture;
    CHECK(capture.windlight_stars_complete);REQUIRE(capture.windlight_star_batch.vertices);
    CHECK(capture.windlight_star_batch.vertices!=old);
    CHECK(f.copied<F32>(*capture.windlight_star_batch.vertices,0,4)[0]==.1f);
    {
        MissingPublisher missing(f.device.get());const U16 indices[]{2,1,0};buffer->setIndexData(indices);buffer->unmapBuffer();
        REQUIRE(buffer->hasPublishedMetalVertexBuffer());REQUIRE_FALSE(buffer->hasPublishedMetalIndexBuffer());
        capture.windlight_star_batch={};pipeline.captureMetalWorldWindLightStarBatch(buffer,mvp);
        CHECK(capture.windlight_stars_complete);CHECK(capture.windlight_star_batch.vertices==buffer->getMetalVertexBuffer());
        REQUIRE(capture.windlight_star_batch.vertices);
        CHECK_FALSE(buffer->hasPublishedMetalIndexBuffer());
    }
    CHECK(f.copied<F32>(*capture.windlight_star_batch.vertices,0,4)[0]==.1f);
    {
        MissingPublisher missing(f.device.get());f.positions(*buffer,false);buffer->unmapBuffer();
        pipeline.captureMetalWorldWindLightStarBatch(buffer,mvp);
        CHECK_FALSE(capture.windlight_stars_complete);CHECK_FALSE(capture.windlight_star_batch.vertices);
    }
    capture.windlight_stars_complete=true;pipeline.captureMetalWorldWindLightStarBatch(buffer,mvp);
    CHECK(capture.windlight_stars_complete);REQUIRE(capture.windlight_star_batch.vertices);
    CHECK(f.copied<F32>(*capture.windlight_star_batch.vertices,0,4)[0]==-1.f);
}

TEST_CASE("Actual opaque capture preserves first-failure and inactive draw admission",
          "[almetal][pipeline-buffer-publication]")
{
    Publication f;Inputs::LLPipeline pipeline;Inputs::LLDrawInfo info;info.mTexture.value=&f.image;
    {
        MissingPublisher missing(f.device.get());info.mVertexBuffer=f.buffer(false);
        REQUIRE_FALSE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
        REQUIRE_FALSE(info.mVertexBuffer->hasPublishedMetalIndexBuffer());
        pipeline.captureMetalWorldGBufferBatch(info,false);
        CHECK_FALSE(pipeline.mMetalWorldSimpleCapture->complete);
        CHECK(pipeline.mMetalWorldSimpleCapture->batches.empty());
    }
    auto& capture=*pipeline.mMetalWorldSimpleCapture;capture.complete=true;
    for(U32 skip=0;skip<5;++skip)
    {
        capture.frame_active=skip!=0;capture.pass_active=skip!=1;capture.complete=skip!=2;
        info.mCount=skip==3?0:3;capture.legacy_glow=skip==4;
        pipeline.captureMetalWorldGBufferBatch(info,false);
        CHECK(capture.batches.empty());CHECK_FALSE(info.mVertexBuffer->hasPublishedMetalVertexBuffer());
    }
    capture.frame_active=capture.pass_active=capture.complete=true;info.mCount=3;capture.legacy_glow=false;
    pipeline.captureMetalWorldGBufferBatch(info,false);
    CHECK(capture.complete);REQUIRE(capture.batches.size()==1);
    CHECK(info.mVertexBuffer->hasPublishedMetalVertexBuffer());CHECK(info.mVertexBuffer->hasPublishedMetalIndexBuffer());
}
