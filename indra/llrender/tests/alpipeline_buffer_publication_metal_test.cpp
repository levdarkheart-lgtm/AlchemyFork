/**
 * @file alpipeline_buffer_publication_metal_test.cpp
 * @brief Actual native capture owners prepare current LLVB stores before copying handles.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include "../../almetal/almetalpipeline.h"
#include "../../almetal/almetalworld.h"
#include "../../almetal/almetalworldmaterialdata.h"
#include "../../almetal/almetalworldresources.h"
#include "../../almetal/almetalshadowprepare.h"
#include "../../almetal/almetalworldvisibility.h"
#include "alnativesourceworkingset.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <thread>
#include <time.h>
#include <utility>
#include <vector>
#include "v4math.h"

namespace
{
namespace Inputs
{
// Explicit viewer frame/material inputs only. The included capture bodies,
// LLVB publisher and typed Metal resources/commands/renderers are real.
struct LLViewerTexture
{
    LLImageGL* image=nullptr;
    mutable U64 image_reads=0;
    LLImageGL* getGLTexture() const{++image_reads;return image;}
    int getDiscardLevel() const{return 0;}
};
template<class T> struct Ref
{
    T* value=nullptr;
    T* get() const{return value;}
    explicit operator bool() const{return value!=nullptr;}
};
struct LLViewerFetchedTexture : LLViewerTexture
{
    inline static Ref<LLViewerFetchedTexture> sWhiteImagep,sDefaultImagep,sFlatNormalImagep;
};
// Typed material inputs only: packed transforms and streamed alpha are fixture
// values. Actual source selection/packing/publication below is production code.
struct LLGLTFMaterial
{
    enum TextureInfo : U32 { GLTF_TEXTURE_INFO_BASE_COLOR, GLTF_TEXTURE_INFO_NORMAL,
        GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS, GLTF_TEXTURE_INFO_EMISSIVE, GLTF_TEXTURE_INFO_COUNT };
    enum { ALPHA_MODE_OPAQUE, ALPHA_MODE_BLEND, ALPHA_MODE_MASK };
    struct TextureTransform
    {
        using Pack=F32[8];
        std::array<F32,8> packed{1.f,1.f,0.f,0.f,0.f,0.f,0.f,0.f};
        void getPacked(Pack& destination) const{std::copy(packed.begin(),packed.end(),destination);}
    };
};
struct LLFetchedGLTFMaterial : LLGLTFMaterial
{
    bool mDoubleSided=false;
    U32 mAlphaMode=ALPHA_MODE_OPAQUE;
    F32 mAlphaCutoff=.5f,mRoughnessFactor=1.f,mMetallicFactor=1.f,streamedAlpha=1.f;
    LLColor4 mBaseColor{1.f,1.f,1.f,1.f};
    LLColor3 mEmissiveColor{0.f,0.f,0.f};
    std::array<TextureTransform,GLTF_TEXTURE_INFO_COUNT> mTextureTransform;
    std::array<LLViewerTexture*,GLTF_TEXTURE_INFO_COUNT> textures{};
    ALNativePublication::Signal signal;
    ALNativePublication::Signal& nativePublication(){return signal;}
    LLViewerTexture* getRenderTexture(TextureInfo slot) const{return textures[slot];}
    F32 getStreamedBaseAlpha() const{return streamedAlpha;}
};
struct LLVOAvatar
{
    struct MatrixPaletteCache
    {
        std::vector<int> mMatrixPalette;std::vector<F32> mGLMp;
        std::shared_ptr<const ALMetalAttachmentPaletteRecord> getMetalAttachmentPalette() const{return {};}
    };
    const MatrixPaletteCache& updateSkinInfoMatrixPalette(void*) { static MatrixPaletteCache unused;return unused; }
};
struct ALViewerMetalShadowSourceCache
{
    std::weak_ptr<const void> native_scene_transient;
    U64 native_scene_revision=0;
    std::weak_ptr<ALMetalWorldVisibility> visibility;
    ALMetalWorldBoundsHandle bounds;
    U64 bounds_revision=0;
    ~ALViewerMetalShadowSourceCache(){if(auto table=visibility.lock())table->retire(bounds);}
};
struct LLDrawInfo
{
    LLPointer<LLVertexBuffer> mVertexBuffer;
    U32 mCount=3,mOffset=0;
    U16 mStart=0,mEnd=2;
    std::vector<Ref<LLViewerTexture>> mTextureList;
    Ref<LLViewerTexture> mTexture,mNormalMap,mSpecularMap;
    struct MaterialSlot
    {
        Ref<LLViewerTexture> mDiffuse,mNormalMap,mSpecularMap;
        LLVector4 mSpecColor{1.f,1.f,1.f,.5f};
        F32 mEnvIntensity=0.f,mAlphaMaskCutoff=.5f,mFullbright=0.f;
    };
    std::vector<MaterialSlot> mMaterialSlotList;
    Ref<LLFetchedGLTFMaterial> mGLTFMaterial;
    Ref<void> mMaterial;
    std::vector<Ref<LLFetchedGLTFMaterial>> mGLTFMaterialList;
    LLVector4 mSpecColor{1.f,1.f,1.f,.5f};
    F32 mEnvIntensity=0.f;
    bool mFullbright=false;
    LLMatrix4* mTextureMatrix=nullptr;
    const LLMatrix4a* mModelMatrix=nullptr;
    LLVOAvatar* mAvatar=nullptr;void* mSkinInfo=nullptr;
    F32 mAlphaMaskCutoff=.5f;
    mutable std::shared_ptr<ALViewerMetalShadowSourceCache> mMetalShadowSource;
    std::shared_ptr<ALNativePublication::State> state=std::make_shared<ALNativePublication::State>();
    mutable U64 publication_reads=0,dependency_walks=0;
    ~LLDrawInfo(){state->invalidate();}
    const std::shared_ptr<ALNativePublication::State>& metalSourcePublication() const{++publication_reads;return state;}
    void observeMetalSourceDependencies() const
    {
        ++dependency_walks;
        // Explicit fixture-frontier wiring uses the REAL producer signals.
        // It does not reproduce the viewer's material/face dependency walk.
        if(mVertexBuffer)mVertexBuffer->nativePublication().observe(state);
        if(mTexture && mTexture.get()->image)mTexture.get()->image->nativePublication().observe(state);
        if(mGLTFMaterial)mGLTFMaterial.get()->signal.observe(state);
    }
};
ALNativeSourceWorkingSet sNativeSceneSources;
std::shared_ptr<ALMetalWorldVisibility> sNativeSceneVisibility;
ALMetalDevice* sNativeSceneVisibilityDevice=nullptr;
U64 sNativeSceneSerial=0;
U32 gFrameCount=1;
#include "../../newview/pipeline_scene_publication_native.inl"
#include "../../newview/pipeline_group_publication_native.inl"
struct Settings {bool native_visibility=false,native_resources=false;} gSavedSettings;
template<class T> struct LLCachedControl
{
    Settings& settings;
    bool resources;
    LLCachedControl(Settings& source,const char* name,T):settings(source),
        resources(std::strcmp(name,"RenderMetalWorldResourceTables")==0){}
    T operator()() const{return static_cast<T>(resources?settings.native_resources:settings.native_visibility);}
};
// Only cull admission is a typed frontier. Native range admission and simple
// capture below are the exact production bodies, including their ordering and
// whole-packet failure behavior. The scalar input list retains its sentinel.
struct LLRenderPass {enum : U32 {PASS_SIMPLE};};
struct LLCullResult
{
    using drawinfo_iterator=LLDrawInfo**;
    struct NativeSimpleRange
    {
        std::shared_ptr<const void> publication;
        U32 legacy_offset=0,draw_count=0;
    };
    std::vector<LLDrawInfo*> raw{nullptr};
    std::array<U32,1> mRenderMapSize{};
    std::vector<NativeSimpleRange> mNativeSimpleRanges;
    U32 mNativeSimpleDrawCount=0;
    void pushDrawInfo(LLDrawInfo* info)
    {raw.insert(raw.end()-1,info);++mRenderMapSize[LLRenderPass::PASS_SIMPLE];}
    drawinfo_iterator beginRenderMap(U32){return raw.data();}
    drawinfo_iterator endRenderMap(U32){return raw.data()+mRenderMapSize[LLRenderPass::PASS_SIMPLE];}
    static void increment_iterator(drawinfo_iterator& at,drawinfo_iterator){++at;}
    const auto& nativeSimpleRanges() const{return mNativeSimpleRanges;}
    U32 getRenderMapSize(U32) const{return mRenderMapSize[LLRenderPass::PASS_SIMPLE]+mNativeSimpleDrawCount;}
    bool pushNativeSimpleRange(std::shared_ptr<const void>,U32) noexcept;
    void clear()
    {mNativeSimpleRanges.clear();mNativeSimpleDrawCount=0;raw.assign(1,nullptr);mRenderMapSize.fill(0);}
};
#include "../../newview/llcullresult_native_ranges.inl"
LLCullResult* sCull=nullptr;
struct WorldResources
{
    std::vector<std::shared_ptr<const void>> owners;
    template<class T> void retain(const std::shared_ptr<T>& value){owners.push_back(value);}
};
struct Capture
{
    bool frame_active=true,pass_active=true,complete=true;
    bool legacy_glow=false,water_exclusion=false;
    bool impostor_alpha_active=false,world_post_deferred_active=false;
    bool impostor_fullbright=false,impostor_shiny=false,impostor_emboss=false;
    bool alpha_mask=false,rigged=false;
    bool material_normal_map=false,material_specular_map=false,pbr_glow=false;
    U32 material_alpha_mode=0;
    U64 scene_source_hits=0,scene_source_builds=0;
    U64 scene_source_failures=0;
    const char* last_scene_source_failure=nullptr;
    ALMetalDevice* device=LLImageGL::getMetalDevice();
    std::vector<ALMetalWorldSimpleBatch> batches;
    std::vector<ALMetalWorldVisibilityMember> world_visibility_members;
    std::shared_ptr<WorldResources> world_resources=std::make_shared<WorldResources>();
    void retainGeometry(LLVertexBuffer& buffer)
    {
        world_resources->retain(buffer.getMetalVertexBufferLease());
        world_resources->retain(buffer.getMetalIndexBufferLease());
    }
    void markIncomplete(U32) {complete=false;}
};
struct LLPipeline
{
    using MetalWorldSimpleCapture=Capture;
    std::unique_ptr<Capture> mMetalWorldSimpleCapture=std::make_unique<Capture>();
    struct {bool isMirrorPass() const{return false;}} mHeroProbeManager;
    void captureMetalWorldGBufferBatch(const LLDrawInfo&,bool);
    void captureMetalPublishedWorldBatch(const LLDrawInfo&,U32,bool);
    void captureMetalWorldSimplePass();
    LLCullResult::drawinfo_iterator beginRenderMap(U32 type){return sCull->beginRenderMap(type);}
    LLCullResult::drawinfo_iterator endRenderMap(U32 type){return sCull->endRenderMap(type);}
};
bool gCubeSnapshot=false;
// Named identity-camera inputs shared identically by raw and group capture.
// These preserve the actual owner model values, including region rebases;
// camera/gGL policy itself is outside this fixture's declared frontier.
void buildMetalWorldDrawMatrices(const LLMatrix4a* model,LLMatrix4a& view,LLMatrix4a& mvp)
{view=model?*model:LLMatrix4a::identity();mvp=view;}
bool buildMetalWorldDrawMatrices(const LLMatrix4a* model,LLMatrix4a& view,LLMatrix4a& inverse,LLMatrix4a& mvp)
{buildMetalWorldDrawMatrices(model,view,mvp);return inverse.setAffineInverse(view);}
void buildMetalWorldDrawMatrices(const LLDrawInfo& info,LLMatrix4a& view,LLMatrix4a& mvp)
{buildMetalWorldDrawMatrices(info.mModelMatrix,view,mvp);}
bool buildMetalWorldDrawMatrices(const LLDrawInfo& info,LLMatrix4a& view,LLMatrix4a& inverse,LLMatrix4a& mvp)
{return buildMetalWorldDrawMatrices(info.mModelMatrix,view,inverse,mvp);}
#include "../../newview/pipeline_buffer_publication_native.inl"
#include "../../newview/pipeline_group_capture_native.inl"
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
    Inputs::LLViewerFetchedTexture image;
    std::string error;
    Publication()
    {
        initRenderer();
        LLPointer<LLImageRaw> raw=new LLImageRaw(1,1,4);
        std::memset(raw->getData(),255,4);white=new LLImageGL(raw,false);
        REQUIRE(white->getMetalTexture());image.image=white;
        Inputs::LLViewerFetchedTexture::sWhiteImagep.value=&image;
        Inputs::LLViewerFetchedTexture::sDefaultImagep.value=&image;
        Inputs::LLViewerFetchedTexture::sFlatNormalImagep.value=&image;
    }
    ~Publication()
    {
        Inputs::sNativeSceneSources.clear();
        Inputs::sNativeSceneVisibility.reset();
        Inputs::sNativeSceneVisibilityDevice=nullptr;
        Inputs::gSavedSettings.native_visibility=false;
        Inputs::gSavedSettings.native_resources=false;
        Inputs::sCull=nullptr;
        Inputs::ALSceneGroupPublicationNative::statistics={};
        Inputs::LLViewerFetchedTexture::sWhiteImagep.value=nullptr;
        Inputs::LLViewerFetchedTexture::sDefaultImagep.value=nullptr;
        Inputs::LLViewerFetchedTexture::sFlatNormalImagep.value=nullptr;
    }
    LLPointer<LLVertexBuffer> buffer(bool require_published=true,U32 extra=0,bool indices32=false)
    {
        LLPointer<LLVertexBuffer> result=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR | extra);
        REQUIRE(result->allocateBuffer(3,indices32?6:3));
        std::memset(result->getMappedData(),0,result->getSize()); // position.w selects texture slot zero.
        positions(*result,false);
        LLStrider<LLVector3> normal;REQUIRE(result->getNormalStrider(normal));
        LLStrider<LLVector2> uv;REQUIRE(result->getTexCoord0Strider(uv));
        LLStrider<LLColor4U> color;REQUIRE(result->getColorStrider(color));
        for(U32 i=0;i<3;++i){normal[i].set(0,0,1);uv[i].set(.5f,.5f);color[i].set(255,0,0,255);}
        if(indices32){const U32 indices[]{0,1,2};result->setIndexData(indices);}
        else {const U16 indices[]{0,1,2};result->setIndexData(indices);}
        result->unmapBuffer();
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

using NativeGroup=Inputs::ALSceneGroupPublicationNative::Payload;
struct CullScope
{
    Inputs::LLCullResult* previous=Inputs::sCull;
    explicit CullScope(Inputs::LLCullResult& cull){Inputs::sCull=&cull;}
    ~CullScope(){Inputs::sCull=previous;}
};
struct GroupPublication : Publication
{
    LLMatrix4a model=LLMatrix4a::identity();
    std::vector<std::unique_ptr<Inputs::LLDrawInfo>> infos;
    std::shared_ptr<ALNativePublication::State> group=std::make_shared<ALNativePublication::State>();
    // Scene-owner relay lifetime only; immutable packets/GPU readers never
    // retain this observer or an LL owner.
    std::shared_ptr<ALNativePublication::Observer> group_dependencies;
    Inputs::LLDrawInfo& draw(const LLColor4U& tint=LLColor4U(255,0,0,255))
    {
        auto value=std::make_unique<Inputs::LLDrawInfo>();
        value->mVertexBuffer=buffer();value->mTexture.value=&image;value->mModelMatrix=&model;
        const std::array<LLColor4U,3> colors{tint,tint,tint};
        value->mVertexBuffer->setColorData(colors.data());value->mVertexBuffer->unmapBuffer();
        infos.push_back(std::move(value));return *infos.back();
    }
    std::vector<Inputs::LLDrawInfo*> draws() const
    {
        std::vector<Inputs::LLDrawInfo*> result;
        for(const auto& info:infos)result.push_back(info.get());
        return result;
    }
    std::shared_ptr<const NativeGroup> seal()
    {
        return seal(Inputs::sNativeSceneSources,Inputs::gFrameCount);
    }
    std::shared_ptr<const NativeGroup> seal(ALNativeSourceWorkingSet& working_set,U32 frame)
    {
        return Inputs::ALSceneGroupPublicationNative::seal(group,group_dependencies,model,draws(),
            working_set,Inputs::sNativeSceneSerial,frame);
    }
    std::shared_ptr<const NativeGroup> current(ALNativeSourceWorkingSet& working_set,U32 frame)
    {
        return Inputs::ALSceneGroupPublicationNative::current(group,model,working_set,frame);
    }
    std::array<U64,3> reads() const
    {
        std::array<U64,3> result{0,0,image.image_reads};
        for(const auto& info:infos)
        {result[0]+=info->publication_reads;result[1]+=info->dependency_walks;}
        return result;
    }
    struct Image
    {
        std::array<std::vector<U8>,4> colors;
        std::vector<F32> depth;
        bool operator==(const Image&) const=default;
    };
    ALMetalWorldGBufferScene scene(const Inputs::Capture& capture) const
    {
        ALMetalWorldGBufferScene result;
        result.batches=capture.batches.data();result.batch_count=capture.batches.size();
        result.source_owner=capture.world_resources;result.width=result.height=side;
        std::fill_n(result.clear_color,4,0.f);return result;
    }
    Image read(const ALMetalRenderTarget& target)
    {
        Image result;
        for(U32 attachment=0;attachment<4;++attachment)
        {
            const auto* texture=target.colorTexture(attachment);REQUIRE(texture);
            const auto row=side*texture->bytesPerPixel();
            result.colors[attachment].resize(side*row);
            REQUIRE(texture->readback(*device,0,0,{},result.colors[attachment].data(),row,&error));
        }
        REQUIRE(target.depthStencilTexture());result.depth.resize(side*side);
        REQUIRE(target.depthStencilTexture()->readback(*device,0,0,{},result.depth.data(),side*sizeof(F32),&error));
        return result;
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

TEST_CASE("Native star publication refreshes vertex stores without requiring index publication",
          "[almetal][pipeline-buffer-publication]")
{
    Publication f; auto buffer=f.buffer();
    const auto* old=buffer->getMetalVertexBuffer();
    f.positions(*buffer,true);
    REQUIRE(buffer->prepareMetalBuffers(false));
    const auto* published=buffer->getMetalVertexBuffer();
    REQUIRE(published); CHECK(published!=old);
    CHECK(f.copied<F32>(*published,0,4)[0]==.1f);
    {
        MissingPublisher missing(f.device.get());
        const U16 indices[]{2,1,0}; buffer->setIndexData(indices); buffer->unmapBuffer();
        REQUIRE(buffer->hasPublishedMetalVertexBuffer());
        REQUIRE_FALSE(buffer->hasPublishedMetalIndexBuffer());
        CHECK(buffer->prepareMetalBuffers(false));
        CHECK(buffer->getMetalVertexBuffer()==published);
        CHECK_FALSE(buffer->hasPublishedMetalIndexBuffer());
    }
    CHECK(f.copied<F32>(*published,0,4)[0]==.1f);
    {
        MissingPublisher missing(f.device.get()); f.positions(*buffer,false); buffer->unmapBuffer();
        CHECK_FALSE(buffer->prepareMetalBuffers(false));
    }
    REQUIRE(buffer->prepareMetalBuffers(false));
    REQUIRE(buffer->getMetalVertexBuffer());
    CHECK(f.copied<F32>(*buffer->getMetalVertexBuffer(),0,4)[0]==-1.f);
}

TEST_CASE("Actual scene factory shares main probe shadow sources and separates animated UV instances",
          "[almetal][pipeline-buffer-publication][scene-publication]")
{
    Publication f; Inputs::LLPipeline pipeline; Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer(); info.mTexture.value=&f.image;
    Inputs::gSavedSettings.native_visibility=true;
    LLMatrix4 animated_uv;
    info.mTextureMatrix=&animated_uv;
    pipeline.captureMetalWorldGBufferBatch(info,false);
    auto& capture=*pipeline.mMetalWorldSimpleCapture;
    REQUIRE(capture.complete); REQUIRE(capture.batches.size()==1);
    REQUIRE(capture.world_visibility_members.size()==1);
    CHECK(capture.world_visibility_members[0].enabled==1);
    CHECK(capture.world_visibility_members[0].identity==info.state->identity());
    const auto initial_bounds_revision=capture.world_visibility_members[0].revision;
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    flavor.visibility_bounds=true;
    auto main=Inputs::ALScenePublicationNative::world(info,flavor,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(main); CHECK(main.reused);CHECK(main.owner->bounds.qualified);
    auto probe=Inputs::ALScenePublicationNative::world(info,flavor,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(probe); CHECK(probe.owner==main.owner); CHECK(probe.prototype==main.prototype);

    unsigned shadow_builds=0;
    const auto build=[&](ALMetalWorldSpotShadowBatch& batch)
    {
        ++shadow_builds;
        auto& vb=*info.mVertexBuffer;
        batch.vertices=vb.getMetalVertexBuffer();batch.indices=vb.getMetalIndexBuffer();
        batch.position_offset=vb.getOffset(LLVertexBuffer::TYPE_VERTEX);
        batch.vertex_count=vb.getNumVerts();batch.index_count=info.mCount;
        batch.index_type=ALMetalIndexType::uint16;
        batch.coverage=ALMetalWorldSpotShadowCoverage::opaque;
        batch.texture_matrix[12]=99.f; // producer-view value must not enter source certificate.
        return true;
    };
    auto shadow=Inputs::ALScenePublicationNative::shadow(info,7,
        ALMetalWorldSpotShadowCoverage::opaque,Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount,build);
    REQUIRE(shadow); REQUIRE(shadow.record); CHECK(shadow_builds==1);
    CHECK((*shadow.record)->prototype().texture_matrix[12]==0.f);
    auto shared=Inputs::ALScenePublicationNative::world(info,flavor,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(shared); CHECK(shared.reused); CHECK(shared.owner==shadow.owner);
    CHECK(shared.prototype->vertices==main.prototype->vertices);
    auto shadow_again=Inputs::ALScenePublicationNative::shadow(info,7,
        ALMetalWorldSpotShadowCoverage::opaque,Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount,build);
    REQUIRE(shadow_again); CHECK(shadow_again.reused); CHECK(shadow_builds==1);
    CHECK(shadow_again.record->get()==shadow.record->get());

    animated_uv.setTranslation(LLVector3(.25f,.5f,0.f));
    pipeline.captureMetalWorldGBufferBatch(info,false);
    REQUIRE(capture.batches.size()==2);
    CHECK(capture.batches[0].texture_matrix[12]==0.f);
    CHECK(capture.batches[1].texture_matrix[12]==.25f);
    CHECK(shared.prototype->texture_matrix[12]==0.f);
    const auto* old_vertices=shared.prototype->vertices;
    f.positions(*info.mVertexBuffer,true);
    CHECK(info.state->dirty()); CHECK_FALSE(info.state->publication());
    pipeline.captureMetalWorldGBufferBatch(info,false);
    REQUIRE(capture.complete); REQUIRE(capture.batches.size()==3);
    REQUIRE(capture.world_visibility_members.size()==3);
    CHECK(capture.world_visibility_members.back().revision!=initial_bounds_revision);
    CHECK(capture.batches.back().vertices!=old_vertices);
    CHECK(f.copied<F32>(*old_vertices,0,4)[0]==-1.f);
    CHECK(f.copied<F32>(*capture.batches.back().vertices,0,4)[0]==.1f);
    const auto old_pixels=f.render(capture.batches[0]);
    const auto new_pixels=f.render(capture.batches.back());
    CHECK(old_pixels[4*Publication::side+2][0]==255);
    CHECK(old_pixels[4*Publication::side+10][0]==0);
    CHECK(new_pixels[4*Publication::side+2][0]==0);
    CHECK(new_pixels[4*Publication::side+10][0]==255);
}

TEST_CASE("Actual scene factory preserves legacy slot gaps and immutable PBR value publications",
          "[almetal][pipeline-buffer-publication][scene-publication]")
{
    Publication f; Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer(true,LLVertexBuffer::MAP_TEXTURE_INDEX|LLVertexBuffer::MAP_TANGENT|
        LLVertexBuffer::MAP_TEXCOORD1|LLVertexBuffer::MAP_TEXCOORD2|LLVertexBuffer::MAP_EMISSIVE);
    info.mTexture.value=&f.image;
    info.mMaterialSlotList.resize(2);
    auto& authored=info.mMaterialSlotList[0];
    authored.mDiffuse.value=&f.image;authored.mSpecColor.set(.1f,.2f,.3f,.4f);
    authored.mEnvIntensity=.6f;authored.mAlphaMaskCutoff=.7f;authored.mFullbright=1.f;
    Inputs::ALScenePublicationNative::WorldFlavor legacy;
    legacy.family=Inputs::ALScenePublicationNative::WorldFamily::legacy;
    legacy.indexed_materials=true;legacy.normal_map=legacy.specular_map=true;
    auto slots=Inputs::ALScenePublicationNative::world(info,legacy,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(slots);REQUIRE(slots.prototype->material_slot_count==2);
    CHECK(slots.prototype->diffuse_textures[1]==f.white->getMetalTexture());
    CHECK(slots.prototype->materialSlots()[0].specular_color[2]==.3f);
    CHECK(slots.prototype->materialSlots()[0].alpha_cutoff==.7f);
    CHECK(slots.prototype->materialSlots()[0].normal_texture==f.white->getMetalTexture());

    Inputs::LLFetchedGLTFMaterial material;
    material.mAlphaMode=Inputs::LLGLTFMaterial::ALPHA_MODE_MASK;
    material.mDoubleSided=true;material.mRoughnessFactor=.4f;material.mMetallicFactor=.2f;
    material.mBaseColor.set(.1f,.2f,.3f,.8f);material.streamedAlpha=.25f;
    material.mEmissiveColor=LLColor3(.11f,.22f,.33f);
    material.mTextureTransform[0].packed={2.f,3.f,.1f,0.f,.2f,.3f,0.f,0.f};
    info.mGLTFMaterial.value=&material;
    info.state->invalidate(); // explicit typed owner binding event, not a fake packing implementation.
    Inputs::ALScenePublicationNative::WorldFlavor pbr;
    pbr.family=Inputs::ALScenePublicationNative::WorldFamily::pbr;pbr.alpha_mask=true;
    auto first=Inputs::ALScenePublicationNative::world(info,pbr,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(first); CHECK(first.prototype->pbr_slot_count==1);CHECK(first.prototype->double_sided);
    CHECK(first.prototype->pbrSlots()[0].base_color_factor[3]==.25f);
    CHECK(first.prototype->pbrSlots()[0].texture_transform[0][1]==3.f);
    CHECK(first.prototype->pbrSlots()[0].roughness_factor==.4f);
    CHECK(first.prototype->pbrSlots()[0].emissive_color[0]==.11f);
    CHECK(first.prototype->pbrSlots()[0].emissive_color[2]==.33f);
    CHECK(first.prototype->pbrSlots()[0].emissive_color[3]==0.f); // PPD-136: adjacent transform starts with 2, never emissive padding.
    material.mRoughnessFactor=.65f;material.signal.didPublish();
    CHECK_FALSE(info.state->publication());
    auto second=Inputs::ALScenePublicationNative::world(info,pbr,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(second);CHECK_FALSE(second.reused);
    CHECK(second.prototype->pbrSlots()[0].roughness_factor==.65f);
    CHECK(first.prototype->pbrSlots()[0].roughness_factor==.4f);
}

TEST_CASE("Actual scene factory preserves GLTF blend coverage and its effective reuse key",
          "[almetal][pipeline-buffer-publication][scene-publication]")
{
    Publication f;
    for(bool indexed:{false,true})
    {
        CAPTURE(indexed);
        Inputs::LLDrawInfo info;Inputs::LLFetchedGLTFMaterial material;
        info.mVertexBuffer=f.buffer(true,indexed?LLVertexBuffer::MAP_TEXTURE_INDEX:0);
        info.mTexture.value=&f.image;
        material.mAlphaMode=Inputs::LLGLTFMaterial::ALPHA_MODE_BLEND;
        material.textures[Inputs::LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR]=&f.image;
        if(indexed)info.mGLTFMaterialList={{&material},{&material}};
        else info.mGLTFMaterial.value=&material;
        unsigned builds=0;
        auto prototype_coverage=ALMetalWorldSpotShadowCoverage::pbr_blend;
        const auto build=[&](ALMetalWorldSpotShadowBatch& batch)
        {
            ++builds;
            auto& vb=*info.mVertexBuffer;
            batch.vertices=vb.getMetalVertexBuffer();batch.indices=vb.getMetalIndexBuffer();
            batch.position_offset=vb.getOffset(LLVertexBuffer::TYPE_VERTEX);
            batch.texcoord_offset=vb.getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
            batch.color_offset=vb.getOffset(LLVertexBuffer::TYPE_COLOR);
            batch.vertex_count=vb.getNumVerts();batch.index_count=info.mCount;
            batch.index_type=ALMetalIndexType::uint16;batch.coverage=prototype_coverage;
            batch.texture_count=indexed?2:1;
            for(U32 slot=0;slot<batch.texture_count;++slot)
            {
                auto& coverage=batch.coverageData();
                coverage.textures[slot]=f.white->getMetalTexture();coverage.alpha_cutoffs[slot]=.37f;
                coverage.pbr_base_color_transforms[slot][0][0]=1.5f;
                coverage.pbr_base_color_transforms[slot][0][1]=1.f;
            }
            return true;
        };
        auto first=Inputs::ALScenePublicationNative::shadow(info,23,
            ALMetalWorldSpotShadowCoverage::legacy_blend,Inputs::sNativeSceneSources,
            Inputs::sNativeSceneSerial,Inputs::gFrameCount,build);
        REQUIRE(first);REQUIRE(first.record);CHECK(builds==1);
        const auto& prototype=(*first.record)->prototype();
        CHECK(prototype.coverage==ALMetalWorldSpotShadowCoverage::pbr_blend);
        CHECK(prototype.texture_count==(indexed?2:1));
        CHECK(prototype.coverageData().alpha_cutoffs[0]==.37f);
        CHECK(prototype.coverageData().pbr_base_color_transforms[0][0][0]==1.5f);
        for(auto requested:{ALMetalWorldSpotShadowCoverage::legacy_blend,ALMetalWorldSpotShadowCoverage::pbr_blend})
        {
            auto reused=Inputs::ALScenePublicationNative::shadow(info,23,requested,
                Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount,build);
            REQUIRE(reused);CHECK(reused.reused);CHECK(builds==1);
            CHECK(reused.owner==first.owner);CHECK(reused.record->get()==first.record->get());
        }
        // A fresh variant still rejects a callback that really emits the wrong family.
        prototype_coverage=ALMetalWorldSpotShadowCoverage::legacy_blend;
        auto wrong=Inputs::ALScenePublicationNative::shadow(info,24,
            ALMetalWorldSpotShadowCoverage::legacy_blend,Inputs::sNativeSceneSources,
            Inputs::sNativeSceneSerial,Inputs::gFrameCount,build);
        CHECK_FALSE(wrong);CHECK(wrong.failure!=nullptr);CHECK(builds==2);
        CHECK(info.state->publication().get()==first.owner.get());
    }
}

TEST_CASE("Actual scene bounds use published indices and fail visible for invalid references",
          "[almetal][pipeline-buffer-publication][scene-publication]")
{
    Publication f;
    for(bool indices32:{false,true})
    {
        CAPTURE(indices32);
        Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer(true,0,indices32);
        info.mTexture.value=&f.image;info.mStart=info.mEnd=0;
        auto& vb=*info.mVertexBuffer;
        REQUIRE(vb.validateRange(info.mStart,info.mEnd,info.mCount,info.mOffset));
        Inputs::ALScenePublicationNative::WorldFlavor flavor;
        auto unqualified=Inputs::ALScenePublicationNative::world(info,flavor,
            Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
        REQUIRE(unqualified);CHECK_FALSE(unqualified.owner->bounds.qualified);
        for(U32 axis=0;axis<4;++axis)
        {
            CHECK(unqualified.owner->bounds.minimum[axis]==0.f);
            CHECK(unqualified.owner->bounds.maximum[axis]==0.f);
        }
        auto default_reuse=Inputs::ALScenePublicationNative::world(info,flavor,
            Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
        REQUIRE(default_reuse);CHECK(default_reuse.reused);CHECK(default_reuse.owner==unqualified.owner);
        flavor.visibility_bounds=true;
        auto valid=Inputs::ALScenePublicationNative::world(info,flavor,
            Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
        REQUIRE(valid);CHECK_FALSE(valid.reused);CHECK(valid.owner->bounds.qualified);
        CHECK(valid.owner!=unqualified.owner);
        CHECK(valid.owner->producer_revision==unqualified.owner->producer_revision);
        CHECK(valid.owner->vertices==unqualified.owner->vertices);
        CHECK(valid.owner->indices==unqualified.owner->indices);
        // Adding the requested bounds variant preserves the earlier immutable
        // reader and its native generation; the default path left extents empty.
        CHECK_FALSE(unqualified.owner->bounds.qualified);
        CHECK(unqualified.owner->bounds.maximum[1]==0.f);
        // Vertex 2 lies outside [mStart,mEnd] and supplies the upper Y extent.
        CHECK(valid.owner->bounds.minimum[0]==-1.f);CHECK(valid.owner->bounds.maximum[0]==-.1f);
        CHECK(valid.owner->bounds.minimum[1]==-1.f);CHECK(valid.owner->bounds.maximum[1]==1.f);
        CHECK(valid.prototype->index_type==(indices32?ALMetalIndexType::uint32:ALMetalIndexType::uint16));
        if(indices32){const U32 indices[]{0,1,std::numeric_limits<U32>::max()};vb.setIndexData(indices);}
        else {const U16 indices[]{0,1,3};vb.setIndexData(indices);}
        vb.unmapBuffer();
        CHECK(info.state->dirty());
        // Preserve the direct path's capacity/range admission contract. Never
        // issue this malformed draw; conservative culling must leave it visible.
        CHECK(vb.validateRange(info.mStart,info.mEnd,info.mCount,info.mOffset));
        auto invalid=Inputs::ALScenePublicationNative::world(info,flavor,
            Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
        REQUIRE(invalid);CHECK_FALSE(invalid.owner->bounds.qualified);
        CHECK(valid.owner->bounds.qualified);CHECK(valid.owner->bounds.maximum[1]==1.f);
        CHECK(invalid.owner->indices!=valid.owner->indices);
    }
}

TEST_CASE("Actual scene factory transient reuse holds no hidden publisher resource lease",
          "[almetal][pipeline-buffer-publication][scene-publication]")
{
    Publication f; Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet tiny(1,1,0);
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    auto first=Inputs::ALScenePublicationNative::world(info,flavor,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(first);CHECK_FALSE(info.state->publication());
    const auto serial=first.owner->serial;
    std::weak_ptr<const Inputs::ALScenePublicationNative::Payload> weak=first.owner;
    auto second=Inputs::ALScenePublicationNative::world(info,flavor,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(second);CHECK(second.reused);CHECK(second.owner==first.owner);
    first={};second={};
    CHECK(weak.expired());
    auto native=info.mVertexBuffer->getMetalVertexBufferLease();
    CHECK(native.use_count()==2); // LLVB plus this actual reader; no source cache copy.
    auto rebuilt=Inputs::ALScenePublicationNative::world(info,flavor,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);
    REQUIRE(rebuilt);CHECK_FALSE(rebuilt.reused);CHECK(rebuilt.owner->serial!=serial);
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

TEST_CASE("Actual native group publication reuses complete sources without warm viewer rediscovery",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;f.draw();f.draw(LLColor4U(0,255,0,255));
    const auto packet=f.seal();REQUIRE(packet);REQUIRE(packet->sources.size()==2);
    CHECK(packet->index_count==6);CHECK_FALSE(f.group->dirty());
    const auto* prototype=packet->sources[0].prototype;
    const auto owner=packet->sources[0].owner;
    const auto reads=f.reads();
    Inputs::LLPipeline pipeline;Inputs::LLCullResult cull;CullScope scope(cull);
    for(U32 view=0;view<3;++view)
    {
        auto warm=f.current(Inputs::sNativeSceneSources,Inputs::gFrameCount);
        REQUIRE(warm==packet);REQUIRE(cull.pushNativeSimpleRange(warm,2));
        CHECK(cull.getRenderMapSize(Inputs::LLRenderPass::PASS_SIMPLE)==2);
        CHECK(cull.beginRenderMap(0)==cull.endRenderMap(0));
        pipeline.captureMetalWorldSimplePass();
        REQUIRE(pipeline.mMetalWorldSimpleCapture->complete);
        REQUIRE(pipeline.mMetalWorldSimpleCapture->batches.size()==2);
        CHECK(pipeline.mMetalWorldSimpleCapture->batches[0].vertices==prototype->vertices);
        CHECK(packet->sources[0].prototype==prototype);CHECK(packet->sources[0].owner==owner);
        CHECK(f.reads()==reads); // No draw-info getter, dependency walk or texture facade read in any warm view.
        cull.clear();pipeline.mMetalWorldSimpleCapture->batches.clear();
        pipeline.mMetalWorldSimpleCapture->world_resources=std::make_shared<Inputs::WorldResources>();
    }
    auto warm_seal=f.seal();CHECK(warm_seal==packet);CHECK(f.reads()==reads);
    CHECK(Inputs::ALSceneGroupPublicationNative::statistics.captured_draws==6);
    REQUIRE(cull.pushNativeSimpleRange(packet,2));
    for(U32 inactive=0;inactive<3;++inactive)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;
        capture.frame_active=inactive!=0;capture.pass_active=inactive!=1;capture.complete=inactive!=2;
        pipeline.captureMetalWorldSimplePass();CHECK(capture.batches.empty());CHECK(f.reads()==reads);
    }
}

TEST_CASE("Actual native raw group raw capture preserves equal-depth MRT depth and query ordering",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;
    auto& before=f.draw(LLColor4U(255,0,0,255));
    auto& first=f.draw(LLColor4U(0,255,0,255));
    auto& second=f.draw(LLColor4U(0,0,255,255));
    auto& after=f.draw(LLColor4U(255,255,0,255));
    f.positions(*second.mVertexBuffer,true);second.mVertexBuffer->unmapBuffer();
    const std::vector<Inputs::LLDrawInfo*> members{&first,&second};
    auto packet=Inputs::ALSceneGroupPublicationNative::seal(f.group,f.group_dependencies,f.model,members,
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(packet);
    Inputs::LLPipeline flat,grouped;Inputs::LLCullResult flat_cull,group_cull;
    for(auto* info:f.draws())flat_cull.pushDrawInfo(info);
    group_cull.pushDrawInfo(&before);REQUIRE(group_cull.pushNativeSimpleRange(packet,2));group_cull.pushDrawInfo(&after);
    CHECK(group_cull.nativeSimpleRanges()[0].legacy_offset==1);
    CHECK(group_cull.getRenderMapSize(0)==4);
    {CullScope scope(flat_cull);flat.captureMetalWorldSimplePass();}
    {CullScope scope(group_cull);grouped.captureMetalWorldSimplePass();}
    const auto& raw=*flat.mMetalWorldSimpleCapture;
    const auto& native=*grouped.mMetalWorldSimpleCapture;
    REQUIRE(raw.complete);REQUIRE(native.complete);REQUIRE(native.batches.size()==4);
    for(U32 draw=0;draw<4;++draw)
    {
        CHECK(native.batches[draw].vertices==raw.batches[draw].vertices);
        CHECK(std::memcmp(native.batches[draw].modelview_projection,raw.batches[draw].modelview_projection,
            sizeof(native.batches[draw].modelview_projection))==0);
    }
    auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
    std::array<ALMetalWorldVisibilityQuery,3> queries{};
    for(auto& query:queries)
    {
        std::memcpy(query.modelview_projection,f.model.getF32ptr(),sizeof(query.modelview_projection));
        query.center[0]=-.75f;query.center[1]=-.5f;query.center[2]=.75f;
        query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;
        query.fan=7;query.count_samples=1;
    }
    queries[1].batch_offset=2;queries[2].batch_offset=4;
    std::array<GroupPublication::Image,2> images;
    std::array<std::array<std::uint64_t,3>,2> samples{};
    for(U32 mode=0;mode<2;++mode)
    {
        auto scene=f.scene(mode?native:raw);scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
        auto target=renderer->render(scene,&f.error,&results);INFO(f.error);REQUIRE(target);REQUIRE(results);
        for(U32 q=0;q<queries.size();++q)REQUIRE(results->wait(q,samples[mode][q],&f.error));
        images[mode]=f.read(*target);
    }
    CHECK(images[0]==images[1]);CHECK(samples[0]==samples[1]);
    CHECK(samples[0][0]>0);CHECK(samples[0][1]==0);CHECK(samples[0][2]==0);
    // LEQUAL makes later coplanar draws visible. Merely comparing black
    // targets would miss omissions or reordering, so assert the last tint.
    const auto pixel=(4*Publication::side+2)*4;
    CHECK(images[1].colors[0][pixel]==255);CHECK(images[1].colors[0][pixel+1]==255);
    CHECK(images[1].colors[0][pixel+2]==0);
    const auto group_pixel=(4*Publication::side+10)*4;
    CHECK(images[1].colors[0][group_pixel]==0);CHECK(images[1].colors[0][group_pixel+1]==0);
    CHECK(images[1].colors[0][group_pixel+2]==255);
}

TEST_CASE("Actual native groups reject incomplete membership and malformed ranges without a prefix",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;auto& first=f.draw();auto& broken=f.draw();f.draw();
    broken.mVertexBuffer=nullptr;
    CHECK_FALSE(f.seal());CHECK(f.group->dirty());CHECK_FALSE(f.group->publication());
    broken.mVertexBuffer=f.buffer();auto packet=f.seal();REQUIRE(packet);REQUIRE(packet->sources.size()==3);
    auto old_dependencies=f.group_dependencies;REQUIRE(old_dependencies);
    broken.state->invalidate();broken.mVertexBuffer=nullptr;
    CHECK_FALSE(f.seal());CHECK_FALSE(f.group->publication());
    CHECK(f.group_dependencies==old_dependencies); // A failed replacement does not publish a partial relay graph.
    broken.mVertexBuffer=f.buffer();auto replacement=f.seal();REQUIRE(replacement);
    CHECK(f.group_dependencies!=old_dependencies);old_dependencies.reset();
    Inputs::LLCullResult cull;
    CHECK_FALSE(cull.pushNativeSimpleRange({},3));CHECK_FALSE(cull.pushNativeSimpleRange(packet,0));
    cull.pushDrawInfo(&first);REQUIRE(cull.pushNativeSimpleRange(packet,3));
    CHECK(cull.getRenderMapSize(0)==4);
    // A corrupt admitted certificate after a good raw prefix must fail the
    // whole captured view, never leave a valid-looking first draw behind.
    cull.mNativeSimpleRanges[0].draw_count=2;
    Inputs::LLPipeline pipeline;
    {CullScope scope(cull);pipeline.captureMetalWorldSimplePass();}
    CHECK_FALSE(pipeline.mMetalWorldSimpleCapture->complete);CHECK(pipeline.mMetalWorldSimpleCapture->batches.empty());
    cull.clear();CHECK(cull.getRenderMapSize(0)==0);CHECK(cull.nativeSimpleRanges().empty());
    cull.mNativeSimpleDrawCount=std::numeric_limits<U32>::max()-1;
    CHECK_FALSE(cull.pushNativeSimpleRange(packet,3));CHECK(cull.nativeSimpleRanges().empty());
    cull.clear();
    auto invalid=std::make_shared<NativeGroup>(*packet);invalid->sources[1]={};
    REQUIRE(cull.pushNativeSimpleRange(invalid,3));
    pipeline.mMetalWorldSimpleCapture->complete=true;
    {CullScope scope(cull);pipeline.captureMetalWorldSimplePass();}
    CHECK_FALSE(pipeline.mMetalWorldSimpleCapture->complete);CHECK(pipeline.mMetalWorldSimpleCapture->batches.empty());
}

TEST_CASE("Actual native group model copies rebase and conservative source gates",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;auto& info=f.draw();auto old=f.seal();REQUIRE(old);
    const auto old_revision=f.group->revision();
    f.model.setTranslation(LLVector3(.25f,.125f,0.f));
    CHECK_FALSE(f.current(Inputs::sNativeSceneSources,Inputs::gFrameCount));
    CHECK(f.group->revision()!=old_revision);CHECK(f.group->dirty());CHECK(old->model[12]==0.f);
    auto rebased=f.seal();REQUIRE(rebased);CHECK(rebased!=old);CHECK(rebased->model[12]==.25f);
    Inputs::LLPipeline flat,native;Inputs::LLCullResult cull;
    flat.captureMetalWorldGBufferBatch(info,true);REQUIRE(cull.pushNativeSimpleRange(rebased,1));
    {CullScope scope(cull);native.captureMetalWorldSimplePass();}
    REQUIRE(flat.mMetalWorldSimpleCapture->complete);REQUIRE(native.mMetalWorldSimpleCapture->complete);
    const auto& a=flat.mMetalWorldSimpleCapture->batches[0];const auto& b=native.mMetalWorldSimpleCapture->batches[0];
    CHECK(a.modelview_projection[12]==.25f);CHECK(a.modelview_projection[13]==.125f);
    CHECK(std::memcmp(a.modelview_projection,b.modelview_projection,sizeof(a.modelview_projection))==0);
    CHECK(std::memcmp(a.normal_matrix,b.normal_matrix,sizeof(a.normal_matrix))==0);
    LLMatrix4 uv;LLMatrix4a other=f.model;Inputs::LLVOAvatar avatar;Inputs::LLFetchedGLTFMaterial material;int skin=0;
    for(U32 gate=0;gate<8;++gate)
    {
        CAPTURE(gate);info.state->invalidate();
        info.mModelMatrix=gate==0?&other:&f.model;info.mTextureMatrix=gate==1?&uv:nullptr;
        info.mAvatar=gate==2?&avatar:nullptr;info.mSkinInfo=gate==3?&skin:nullptr;
        info.mCount=gate==4?0:3;info.mGLTFMaterial.value=gate==5?&material:nullptr;
        info.mMaterial.value=gate==6?&skin:nullptr;info.mMaterialSlotList.resize(gate==7?1:0);
        CHECK_FALSE(f.seal());CHECK_FALSE(f.group->publication());CHECK(f.group->dirty());
    }
}

TEST_CASE("Actual source before-write drops native group cache before COW but encoded views pin old GPU bytes",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;auto& info=f.draw();auto packet=f.seal();REQUIRE(packet);
    auto* original=info.mVertexBuffer->getMetalVertexBuffer();
    packet.reset();gGL.frameFences().release();
    const std::array<LLColor4U,3> green{LLColor4U(0,255,0,255),LLColor4U(0,255,0,255),LLColor4U(0,255,0,255)};
    const std::array<LLColor4U,3> blue{LLColor4U(0,0,255,255),LLColor4U(0,0,255,255),LLColor4U(0,0,255,255)};
    const auto before=LLVertexBuffer::metalUploadStats();
    info.mVertexBuffer->setColorData(green.data());
    CHECK(info.state->dirty());CHECK_FALSE(info.state->publication());
    CHECK(f.group->dirty());CHECK_FALSE(f.group->publication());
    info.mVertexBuffer->unmapBuffer();
    CHECK(info.mVertexBuffer->getMetalVertexBuffer()==original);
    CHECK(LLVertexBuffer::metalUploadStats().leased_orphans==before.leased_orphans);

    packet=f.seal();REQUIRE(packet);
    std::weak_ptr<const NativeGroup> weak=packet;
    Inputs::LLCullResult cull;REQUIRE(cull.pushNativeSimpleRange(packet,1));
    Inputs::LLPipeline pipeline;
    {CullScope scope(cull);pipeline.captureMetalWorldSimplePass();}
    auto& capture=*pipeline.mMetalWorldSimpleCapture;REQUIRE(capture.complete);REQUIRE(capture.batches.size()==1);
    auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
    REQUIRE(renderer->beginCommandBatch("Actual group encoded reader",&f.error));
    auto scene=f.scene(capture);auto target=renderer->render(scene,&f.error);INFO(f.error);REQUIRE(target);
    auto completion=renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
    // The native command is deliberately uncommitted, so this is an actual
    // encoded reader with deterministic lifetime, not a synthetic GPU gate.
    scene={};cull.clear();packet.reset();capture.batches.clear();
    capture.world_resources=std::make_shared<Inputs::WorldResources>();
    f.group->invalidate();info.state->invalidate();gGL.frameFences().release();
    CHECK_FALSE(weak.expired());
    info.mVertexBuffer->setColorData(blue.data());info.mVertexBuffer->unmapBuffer();
    CHECK(info.mVertexBuffer->getMetalVertexBuffer()!=original);
    CHECK(LLVertexBuffer::metalUploadStats().leased_orphans==before.leased_orphans+1);
    CHECK(f.copied<U8>(*original,info.mVertexBuffer->getOffset(LLVertexBuffer::TYPE_COLOR),4)[1]==255);
    REQUIRE(renderer->commitCommandBatch(&f.error));REQUIRE(f.device->waitUntilIdle(&f.error));
    CHECK(completion->isComplete());
    const auto old=f.read(*target);const auto pixel=(4*Publication::side+2)*4;
    CHECK(old.colors[0][pixel]==0);CHECK(old.colors[0][pixel+1]==255);CHECK(old.colors[0][pixel+2]==0);
    // Drop completed command wrappers too; GPU completion is sufficient for
    // safety even if the renderer retains its completed bookkeeping briefly.
    renderer.reset();
    const auto retired_by=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!weak.expired() && std::chrono::steady_clock::now()<retired_by)std::this_thread::yield();
    CHECK(weak.expired());
    Inputs::LLPipeline fresh;fresh.captureMetalWorldGBufferBatch(info,true);
    REQUIRE(fresh.mMetalWorldSimpleCapture->complete);REQUIRE(fresh.mMetalWorldSimpleCapture->batches.size()==1);
    auto current=f.render(fresh.mMetalWorldSimpleCapture->batches[0]);
    CHECK(current[4*Publication::side+2][2]==255);CHECK(current[4*Publication::side+2][1]==0);
}

TEST_CASE("Actual native group invalidation eviction destruction and transient retry retain only real readers",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;auto& info=f.draw();auto packet=f.seal();REQUIRE(packet);
    const auto child_revision=info.state->revision();
    std::weak_ptr<const NativeGroup> weak=packet;
    info.state->evict(); // PPD-160: child-cache eviction is distinct from producer mutation.
    CHECK(info.state->revision()==child_revision);CHECK(f.group->publication().get()==packet.get());CHECK_FALSE(f.group->dirty());
    CHECK_FALSE(weak.expired());CHECK(packet->sources[0].prototype->vertices==info.mVertexBuffer->getMetalVertexBuffer());
    packet.reset();CHECK_FALSE(weak.expired());
    packet=f.seal();REQUIRE(packet);weak=packet;
    // Explicit group-owner clear/destroy event: source observers are weak,
    // while this already-admitted immutable view is independently owned.
    f.group->invalidate();CHECK_FALSE(f.group->publication());
    f.group.reset();CHECK_FALSE(weak.expired());
    packet.reset();CHECK(weak.expired());
    f.group=std::make_shared<ALNativePublication::State>();packet=f.seal();REQUIRE(packet);
    weak=packet;packet.reset();
    info.state->invalidate(); // Actual LLDrawInfo destructor uses this same source event.
    CHECK_FALSE(f.group->publication());CHECK(weak.expired());
    packet=f.seal();REQUIRE(packet);weak=packet;packet.reset();
    f.infos.clear(); // The fixture's LLDrawInfo destructor invalidates its actual State.
    CHECK_FALSE(f.group->publication());CHECK(weak.expired());

    auto& transient_info=f.draw();
    ALNativeSourceWorkingSet tiny(1,1,0);
    auto transient_group=std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> transient_dependencies;
    const std::vector<Inputs::LLDrawInfo*> members{&transient_info};
    CHECK_FALSE(Inputs::ALSceneGroupPublicationNative::seal(transient_group,transient_dependencies,f.model,members,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount));
    CHECK_FALSE(transient_group->publication());CHECK(transient_group->dirty());
    CHECK_FALSE(transient_dependencies);
    // An actual raw reader remains valid outside cache residency. Retrying
    // with that reader cannot turn an unbudgeted group into a publisher.
    Inputs::ALScenePublicationNative::WorldFlavor flavor;flavor.indexed_textures=true;
    auto raw_reader=Inputs::ALScenePublicationNative::world(transient_info,flavor,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(raw_reader);
    CHECK_FALSE(transient_info.state->publication());
    auto rejected=Inputs::ALSceneGroupPublicationNative::seal(transient_group,transient_dependencies,f.model,members,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);CHECK_FALSE(rejected);
    CHECK_FALSE(transient_group->publication());CHECK_FALSE(transient_dependencies);
    CHECK(tiny.entryCount()==0);
    ALNativeSourceWorkingSet enough(4,1024*1024,2);
    auto retry=Inputs::ALSceneGroupPublicationNative::seal(transient_group,transient_dependencies,f.model,members,enough,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(retry);
    CHECK(retry->sources[0].owner==raw_reader.owner);CHECK(retry->sources[0].prototype==raw_reader.prototype);
    CHECK_FALSE(transient_info.state->publication());CHECK(enough.entryCount()==1);
    CHECK(enough.estimatedRetainedBytes()>1);CHECK(transient_dependencies);
}

TEST_CASE("Actual active native group stays budgeted beyond child cache age without rediscovery",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;auto& first=f.draw();auto& second=f.draw();
    second.mVertexBuffer=first.mVertexBuffer; // Two sources, one actual native allocation tuple.
    ALNativeSourceWorkingSet bounded(8,1024*1024,2);
    auto packet=f.seal(bounded,1);REQUIRE(packet);REQUIRE(packet->sources.size()==2);
    const auto bytes=first.mVertexBuffer->getMetalVertexBuffer()->length()+
        first.mVertexBuffer->getMetalIndexBuffer()->length()+
        Inputs::ALScenePublicationNative::textureBytes(*f.white->getMetalTexture());
    CHECK(bounded.entryCount()==3);CHECK(bounded.estimatedRetainedBytes()==bytes);
    CHECK(packet->resources.size()==3); // Group/source entries account shared control blocks once.
    const auto serial=packet->serial,revision=f.group->revision();
    const auto child_revision=first.state->revision();const auto reads=f.reads();
    for(U32 frame=2;frame<=8;++frame)
    {
        auto active=f.current(bounded,frame);REQUIRE(active==packet);
        bounded.trim(frame); // Same owner ordering: the admitted group is touched before trimming.
        CHECK(f.group->publication().get()==packet.get());CHECK(f.group->revision()==revision);
        CHECK(active->serial==serial);CHECK(bounded.estimatedRetainedBytes()==bytes);
        CHECK(f.reads()==reads);
        if(frame>=4)
        {
            CHECK_FALSE(first.state->publication());CHECK_FALSE(second.state->publication());
            CHECK(first.state->revision()==child_revision);CHECK(bounded.entryCount()==1);
        }
    }
    // Residency withdrawal ignored by the relay must not also suppress a
    // real before-write event after both child cache entries have aged out.
    first.state->invalidate();CHECK_FALSE(f.group->publication());CHECK(f.group->dirty());
    // As for source entries, stale conservative accounting is removed on
    // its next ledger validation; invalidation itself does not walk the LRU.
    CHECK_FALSE(bounded.touch(f.group->identity(),serial,9));
    CHECK_FALSE(f.current(bounded,9));CHECK(bounded.entryCount()==0);
    CHECK(bounded.estimatedRetainedBytes()==0);CHECK(packet->sources[0].prototype->vertices);
}

TEST_CASE("Actual native group byte count and age eviction drop only publisher residency",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    for(U32 reason=0;reason<3;++reason)
    {
        CAPTURE(reason);GroupPublication f;auto& info=f.draw();
        ALNativeSourceWorkingSet bounded(reason==1?2:8,1024*1024,reason==2?2:100);
        auto packet=f.seal(bounded,1);REQUIRE(packet);CHECK(bounded.entryCount()==2);
        const auto* vertices=packet->sources[0].prototype->vertices;
        std::weak_ptr<const NativeGroup> weak=packet;
        if(reason==0)bounded.setByteBudget(0);
        if(reason==1)
        {
            // Fill current-frame admission separately from older entries.
            // First evict the child source, then the independently tracked
            // group, proving its retention consumes a real count slot.
            auto a=std::make_shared<ALNativePublication::State>();auto av=std::make_shared<int>(1);
            REQUIRE(a->publish(a->revision(),av));REQUIRE(bounded.retain(a,1,av,{},2));
            CHECK_FALSE(info.state->publication());CHECK(f.group->publication().get()==packet.get());
            auto b=std::make_shared<ALNativePublication::State>();auto bv=std::make_shared<int>(2);
            REQUIRE(b->publish(b->revision(),bv));REQUIRE(bounded.retain(b,1,bv,{},3));
        }
        if(reason==2)bounded.trim(4);
        CHECK_FALSE(f.group->publication());CHECK(f.group->dirty());
        CHECK_FALSE(weak.expired());CHECK(packet->sources[0].prototype->vertices==vertices);
        CHECK(bounded.estimatedRetainedBytes()==0);
        // A held immutable view still owns the exact resource generation.
        CHECK(f.copied<F32>(*vertices,packet->sources[0].prototype->position_offset,4)[0]==-1.f);
        packet.reset();CHECK(weak.expired());
    }
}

TEST_CASE("Actual native group full capture and encode costs include cold warm churn and three views",
          "[almetal][.native-group-cost]")
{
    // Deliberately opt-in. This measures actual owner/source publication,
    // ordered cull admission, capture, native encode and completion envelopes.
    // Mesh/texture creation and the identity camera are explicit fixture inputs;
    // no assertion or label converts these bounded costs into viewer FPS.
    GroupPublication f;
    auto main=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);
    auto probes=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);
    auto frame_data=ALMetalWorldFrameData::create(*f.device,&f.error);
    INFO(f.error);REQUIRE(main);REQUIRE(probes);REQUIRE(frame_data);
    main->setFrameData(frame_data);probes->setFrameData(frame_data);
    struct Group
    {
        std::shared_ptr<ALNativePublication::State> state=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> dependencies;
        std::vector<Inputs::LLDrawInfo*> members;
    };
    struct Arm
    {
        std::vector<Group> groups;
        Inputs::LLPipeline pipeline;
        Inputs::LLCullResult cull;
    };
    const auto cpu_ns=[]
    {
        timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
        return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;
    };
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return values[values.size()/2];};
    constexpr U32 repetitions=5,warmups=2,draws_per_group=16;
    for(U32 count:{1u,4u,8u,16u,512u})
    {
        std::array<Arm,2> arms;
        for(auto& arm:arms)
            for(U32 first=0;first<count;first+=draws_per_group)
            {
                auto& group=arm.groups.emplace_back();
                for(U32 i=first;i<std::min(count,first+draws_per_group);++i)
                    group.members.push_back(&f.draw(LLColor4U(255,0,0,255)));
            }
        for(U32 views:{1u,3u})for(U32 condition=0;condition<3;++condition)
        {
            for(auto& arm:arms)for(auto& group:arm.groups)
            {group.state->invalidate();for(auto* info:group.members)info->state->invalidate();}
            struct Costs
            {
                std::array<double,repetitions> cpu{},encode{},complete{},gpu{};
                U64 group_draws=0,source_publications=0,source_reads=0,dependency_walks=0,image_reads=0;
                U64 group_seals=0,group_declines=0,published_bytes=0,gpu_ready=0;
            };
            std::array<Costs,2> costs;
            for(U32 sample=0;sample<warmups+repetitions;++sample)
            {
                std::array<std::array<GroupPublication::Image,3>,2> verified;
                // Alternate the pair's order to reduce a fixed warm-cache or
                // thermal ordering advantage without mixing their producers.
                for(U32 position=0;position<2;++position)
                {
                    const U32 mode=(sample+position)%2;auto& arm=arms[mode];
                    const auto before_reads=f.reads();const auto source_serial=Inputs::sNativeSceneSerial;
                    const auto bytes_before=frame_data->stats().published_bytes;
                    U64 admitted=0,seals=0,declines=0;
                    std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                    std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                    const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                    ++Inputs::gFrameCount;frame_data->beginFrame(Inputs::gFrameCount);
                    if(condition==0)
                    {
                        // Cold source/packet caches, with the producer event
                        // and every publication/admission inside the interval.
                        for(auto& group:arm.groups)
                        {group.state->invalidate();for(auto* info:group.members)info->state->invalidate();}
                    }
                    if(condition==2)
                    {
                        // One real LLVB color update per actual group (up to
                        // 16 records, including the short 1/4/8-record cases). Its
                        // signal, source/group invalidation and native upload
                        // are charged, not replaced by a prebuilt prototype.
                        const LLColor4U tint=sample%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                        const std::array<LLColor4U,3> changed{tint,tint,tint};
                        for(auto& group:arm.groups)
                        {auto& vb=*group.members.front()->mVertexBuffer;vb.setColorData(changed.data());vb.unmapBuffer();}
                    }
                    for(U32 view=0;view<views;++view)
                    {
                        auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                        capture.complete=true;capture.batches.clear();capture.world_visibility_members.clear();
                        capture.world_resources=std::make_shared<Inputs::WorldResources>();arm.cull.clear();
                        for(auto& group:arm.groups)
                        {
                            const auto packet=mode?Inputs::ALSceneGroupPublicationNative::current(group.state,f.model,
                                Inputs::sNativeSceneSources,Inputs::gFrameCount):nullptr;
                            if(packet && arm.cull.pushNativeSimpleRange(packet,static_cast<U32>(packet->sources.size())))
                                admitted+=packet->sources.size();
                            else for(auto* info:group.members)arm.cull.pushDrawInfo(info);
                        }
                        if(mode)
                        {
                            // Production admits warm packets first, then
                            // flushes/seals dirty producers for a later view.
                            LLVertexBuffer::flushBuffers();
                            for(auto& group:arm.groups)if(group.state->dirty())
                            {
                                ++seals;
                                if(!Inputs::ALSceneGroupPublicationNative::seal(group.state,group.dependencies,f.model,group.members,
                                    Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount))++declines;
                            }
                        }
                        {CullScope scope(arm.cull);arm.pipeline.captureMetalWorldSimplePass();}
                        REQUIRE(capture.complete);REQUIRE(capture.batches.size()==count);
                        auto& renderer=view==0?*main:*probes;
                        REQUIRE(renderer.beginCommandBatch("Actual group publication path cost",&f.error));
                        auto scene=f.scene(capture);targets[view]=renderer.render(scene,&f.error);INFO(f.error);REQUIRE(targets[view]);
                        completions[view]=renderer.commandBatchCompletion();
                        REQUIRE(renderer.commitCommandBatch(&f.error));
                    }
                    const auto owner_cpu=cpu_ns()-begin_cpu;
                    const auto encoded=std::chrono::steady_clock::now();
                    REQUIRE(f.device->waitUntilIdle(&f.error));
                    const auto completed=std::chrono::steady_clock::now();
                    // Every view of the frame shares ONE frame-data epoch.
                    // Only completed GPU work permits fence release/CPU reset.
                    gGL.frameFences().release();
                    if(sample==warmups-1)
                        for(U32 view=0;view<views;++view)verified[mode][view]=f.read(*targets[view]);
                    if(sample>=warmups)
                    {
                        auto& cost=costs[mode];const U32 slot=sample-warmups;
                        const auto after_reads=f.reads();
                        cost.cpu[slot]=owner_cpu/1e6;
                        cost.encode[slot]=std::chrono::duration<double,std::milli>(encoded-begin).count();
                        cost.complete[slot]=std::chrono::duration<double,std::milli>(completed-begin).count();
                        for(U32 view=0;view<views;++view)
                        {
                            ALMetalCommandGPUTime gpu;
                            if(completions[view] && completions[view]->gpuTime(gpu)==ALMetalCommandTimingStatus::ready)
                            {cost.gpu[slot]+=gpu.elapsed_nanoseconds/1e6;++cost.gpu_ready;}
                        }
                        cost.group_draws+=admitted;cost.group_seals+=seals;cost.group_declines+=declines;
                        cost.source_publications+=Inputs::sNativeSceneSerial-source_serial;
                        cost.source_reads+=after_reads[0]-before_reads[0];cost.dependency_walks+=after_reads[1]-before_reads[1];
                        cost.image_reads+=after_reads[2]-before_reads[2];
                        cost.published_bytes+=frame_data->stats().published_bytes-bytes_before;
                        if(mode && condition==1)CHECK(admitted==U64(count)*views);
                        CHECK(declines==0);
                    }
                    arm.cull.clear();arm.pipeline.mMetalWorldSimpleCapture->batches.clear();
                    arm.pipeline.mMetalWorldSimpleCapture->world_resources=std::make_shared<Inputs::WorldResources>();
                }
                if(sample==warmups-1)
                    for(U32 view=0;view<views;++view)CHECK(verified[0][view]==verified[1][view]);
            }
            for(U32 mode=0;mode<2;++mode)
            {
                const auto& cost=costs[mode];
                std::cout<<"NATIVE_GROUP_PATH_COST draws_per_view="<<count<<" views="<<views
                    <<" records_per_group="<<std::min(count,draws_per_group)
                    <<" groups_per_view="<<(count+draws_per_group-1)/draws_per_group
                    <<" condition="<<(condition==0?"cold":condition==1?"warm":"churn_one_per_group")
                    <<" updated_records_per_view="<<(condition==2?(count+draws_per_group-1)/draws_per_group:0)
                    <<" mode="<<(mode?"native_group":"flat_per_draw")<<" repetitions="<<repetitions<<" warmups="<<warmups
                    <<" owner_cpu_median_ms="<<median(cost.cpu)<<" capture_encode_median_ms="<<median(cost.encode)
                    <<" completion_envelope_median_ms="<<median(cost.complete)<<" gpu_sum_median_ms="<<median(cost.gpu)
                    <<" gpu_ready_commands="<<cost.gpu_ready<<" admitted_native_draws="<<cost.group_draws
                    <<" group_seal_attempts="<<cost.group_seals<<" group_declines="<<cost.group_declines
                    <<" source_and_group_serials="<<cost.source_publications<<" source_getter_reads="<<cost.source_reads
                    <<" dependency_walks="<<cost.dependency_walks<<" image_getter_reads="<<cost.image_reads
                    <<" native_frame_bytes="<<cost.published_bytes
                    <<" geometry_creation_excluded=1 identity_camera=1 whole_viewer_fps_claim=0\n";
            }
        }
    }
}
