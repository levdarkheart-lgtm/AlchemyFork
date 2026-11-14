/**
 * @file alpipeline_buffer_publication_metal_test.cpp
 * @brief Actual native capture owners prepare current LLVB stores before copying handles.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include "../../almetal/almetalpipeline.h"
#include "../../almetal/almetalworld.h"
#include "../../almetal/almetalworldcohort.h"
#include "../../almetal/almetalworldprepare.h"
#include "../../almetal/almetalworldmaterialdata.h"
#include "../../almetal/almetalworldresources.h"
#include "../../almetal/almetalworldsourceview.h"
#include "../../newview/alnativeregionmodel.h"
#include "../../almetal/almetalshadowprepare.h"
#include "../../almetal/almetalworldvisibility.h"
#include "alnativesourceworkingset.h"
#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <thread>
#include <time.h>
#include <utility>
#include <unordered_set>
#include <unordered_map>
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
struct LLRenderPass {enum : U32 {PASS_SIMPLE, PASS_MATERIAL, PASS_GLTF_PBR, NUM_RENDER_TYPES};};
struct LLCullResult
{
    using drawinfo_iterator=LLDrawInfo**;
    struct NativeSimpleRange
    {
        std::shared_ptr<const void> publication;
        U32 legacy_offset=0,draw_count=0;
    };
    std::vector<LLDrawInfo*> raw{nullptr};
    std::array<U32,LLRenderPass::NUM_RENDER_TYPES> mRenderMapSize{};
    std::array<std::vector<LLDrawInfo*>,LLRenderPass::NUM_RENDER_TYPES-1> material_raw;
    struct NativeMaterialRange
    {
        std::shared_ptr<const void> publication;U32 legacy_offset=0,draw_count=0;
        ALNativeRegionModel::Snapshot model;
        U32 raw_skip=0;
    };
    std::map<U32,std::vector<NativeMaterialRange>> mNativeMaterialRanges;
    std::array<U32,LLRenderPass::NUM_RENDER_TYPES> mNativeMaterialDrawCount{};
    std::vector<NativeSimpleRange> mNativeSimpleRanges;
    U32 mNativeSimpleDrawCount=0;
    LLCullResult(){for(auto& values:material_raw)values.assign(1,nullptr);}
    auto& rawMap(U32 type){return type==LLRenderPass::PASS_SIMPLE?raw:material_raw[type-1];}
    void pushDrawInfo(U32 type,LLDrawInfo* info)
    {auto& values=rawMap(type);values.insert(values.end()-1,info);++mRenderMapSize[type];}
    void pushDrawInfo(LLDrawInfo* info){pushDrawInfo(LLRenderPass::PASS_SIMPLE,info);}
    drawinfo_iterator beginRenderMap(U32 type){return rawMap(type).data();}
    drawinfo_iterator endRenderMap(U32 type){return rawMap(type).data()+mRenderMapSize[type];}
    static void increment_iterator(drawinfo_iterator& at,drawinfo_iterator){++at;}
    const auto& nativeSimpleRanges() const{return mNativeSimpleRanges;}
    const auto& nativeMaterialRanges(U32 type) const
    {static const std::vector<NativeMaterialRange> empty;auto at=mNativeMaterialRanges.find(type);return at==mNativeMaterialRanges.end()?empty:at->second;}
    U32 getRenderMapSize(U32 type) const{return mRenderMapSize[type]+(type==LLRenderPass::PASS_SIMPLE?mNativeSimpleDrawCount:mNativeMaterialDrawCount[type]);}
    bool pushNativeSimpleRange(std::shared_ptr<const void>,U32) noexcept;
    bool pushNativeMaterialRange(U32,std::shared_ptr<const void>,U32,const ALNativeRegionModel::Snapshot&) noexcept;
    bool replaceNativeMaterialRange(U32,std::shared_ptr<const void>,U32,std::span<LLDrawInfo* const>,const ALNativeRegionModel::Snapshot&) noexcept;
    void clear()
    {mNativeSimpleRanges.clear();mNativeSimpleDrawCount=0;raw.assign(1,nullptr);mRenderMapSize.fill(0);
     mNativeMaterialRanges.clear();mNativeMaterialDrawCount.fill(0);for(auto& values:material_raw)values.assign(1,nullptr);}
};
#include "../../newview/llcullresult_native_ranges.inl"
LLCullResult* sCull=nullptr;
struct WorldResources
{
    std::vector<std::shared_ptr<const void>> owners;
    std::unordered_set<const void*> identities;
    void retain(std::shared_ptr<const void> owner)
    {
        if(!owner)return;
        const auto [at,inserted]=identities.insert(owner.get());
        if(inserted)
        {
            try{owners.push_back(std::move(owner));}
            catch(...){identities.erase(at);throw;}
        }
    }
};
#include "../../newview/pipeline_cpu_source_lease_native.inl"
struct Capture
{
    using WorldResourceLease=WorldResources;
    bool frame_active=true,pass_active=true,complete=true;
    bool legacy_glow=false,water_exclusion=false,direct_probe_scene_active=false;
    bool impostor_alpha_active=false,world_post_deferred_active=false,impostor_bake=false;
    bool impostor_fullbright=false,impostor_shiny=false,impostor_emboss=false;
    bool alpha_mask=false,rigged=false,pbr=true;
    bool material_normal_map=false,material_specular_map=false,pbr_glow=false;
    U32 material_alpha_mode=0;
    U64 scene_source_hits=0,scene_source_builds=0;
    U64 scene_source_failures=0;
    const char* last_scene_source_failure=nullptr;
    ALMetalDevice* device=LLImageGL::getMetalDevice();
    std::vector<ALMetalWorldSimpleBatch> batches;
    std::vector<ALMetalWorldSourceReference> source_references;
    std::vector<ALMetalWorldDrawIndex> draw_order;
    std::size_t raw_draw_watermark=0;
    std::shared_ptr<WorldCpuLease> cpu_sources;
    bool compact_gbuffer_active=false;
    std::vector<ALMetalWorldVisibilityMember> world_visibility_members;
    std::shared_ptr<WorldResources> world_resources=std::make_shared<WorldResources>();
#include "../../newview/pipeline_source_rows_capture_native.inl"
#include "../../newview/pipeline_compact_capture_native.inl"
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
    void captureMetalWorldGBufferBatch(const LLDrawInfo&,bool,bool editable=false);
    void captureMetalPublishedWorldBatch(const LLDrawInfo&,U32,bool,bool editable=false);
    void captureMetalWorldSimplePass();
    void captureMetalWorldMaterialPass(U32);
    void captureMetalWorldPBRPass(U32);
    void captureMetalWorldPublishedPass(U32,U32);
    void captureMetalWorldMaterialBatch(const LLDrawInfo& info,bool indexed){captureMetalPublishedWorldBatch(info,1,indexed);}
    void captureMetalWorldPBRBatch(const LLDrawInfo& info,bool indexed)
    {if(mMetalWorldSimpleCapture->pbr)captureMetalPublishedWorldBatch(info,2,indexed);}
    LLCullResult::drawinfo_iterator beginRenderMap(U32 type){return sCull->beginRenderMap(type);}
    LLCullResult::drawinfo_iterator endRenderMap(U32 type){return sCull->endRenderMap(type);}
};
bool gCubeSnapshot=false;
U64 matrix_builds=0;
// Named identity-camera inputs shared identically by raw and group capture.
// These preserve the actual owner model values, including region rebases;
// camera/gGL policy itself is outside this fixture's declared frontier.
void buildMetalWorldDrawMatrices(const LLMatrix4a* model,LLMatrix4a& view,LLMatrix4a& mvp)
{++matrix_builds;view=model?*model:LLMatrix4a::identity();mvp=view;}
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
    std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> group_layout;
    Inputs::LLDrawInfo& draw(const LLColor4U& tint=LLColor4U(255,0,0,255))
    {
        auto value=std::make_unique<Inputs::LLDrawInfo>();
        value->mVertexBuffer=buffer();value->mTexture.value=&image;value->mModelMatrix=&model;
        const std::array<LLColor4U,3> colors{tint,tint,tint};
        value->mVertexBuffer->setColorData(colors.data());value->mVertexBuffer->unmapBuffer();
        if(group_layout)
        {group->invalidate();group_layout.reset();group_dependencies.reset();}
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
        return Inputs::ALSceneGroupPublicationNative::seal(group,group_dependencies,group_layout,model,draws(),
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
        result.references=capture.source_references.data();result.reference_count=capture.source_references.size();
        result.draw_order=capture.draw_order.data();result.draw_order_count=capture.draw_order.size();
        result.cohorts=capture.source_cohorts.data();result.cohort_count=capture.source_cohorts.size();
        result.cpu_source_owner=capture.cpu_sources;
        result.source_rows=capture.source_rows;
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

struct MaterialGroupPublication : GroupPublication
{
    ALNativeRegionModel region;
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    std::vector<std::unique_ptr<Inputs::LLFetchedGLTFMaterial>> materials;
    U32 type=Inputs::LLRenderPass::PASS_MATERIAL;
    MaterialGroupPublication(U32 family=1,U32 mask=0)
    {
        REQUIRE(region.update(model,LLVector3(0.f,0.f,0.f)));
        flavor.family=static_cast<Inputs::ALScenePublicationNative::WorldFamily>(family);
        flavor.native_materials=flavor.native_resources=true;
        if(family==1){flavor.normal_map=(mask&8)!=0;flavor.specular_map=(mask&4)!=0;flavor.material_alpha_mode=mask&3;}
        else {type=Inputs::LLRenderPass::PASS_GLTF_PBR;flavor.alpha_mask=mask!=0;}
        Inputs::gSavedSettings.native_resources=true;
    }
    ~MaterialGroupPublication()
    {
        if(group)group->invalidate();
        group_layout.reset();group_dependencies.reset();infos.clear();
    }
    Inputs::LLDrawInfo& materialDraw(bool indexed=false)
    {
        auto& info=draw();U32 extra=indexed?LLVertexBuffer::MAP_TEXTURE_INDEX:0;
        if(flavor.normal_map || flavor.family==Inputs::ALScenePublicationNative::WorldFamily::pbr)extra|=LLVertexBuffer::MAP_TANGENT;
        if(flavor.normal_map)extra|=LLVertexBuffer::MAP_TEXCOORD1;
        if(flavor.specular_map)extra|=LLVertexBuffer::MAP_TEXCOORD2;
        info.mVertexBuffer=buffer(true,extra);
        if(extra&LLVertexBuffer::MAP_TANGENT)
        {
            std::array<LLVector4a,3> tangents;for(auto& tangent:tangents)tangent.set(1.f,0.f,0.f,1.f);
            info.mVertexBuffer->setTangentData(tangents.data());
        }
        if(indexed)
        {
            std::array<LLVector4a,3> positions;
            positions[0].set(-1.f,-1.f,0.f,std::bit_cast<float>(1u));
            positions[1].set(-.1f,-1.f,0.f,std::bit_cast<float>(1u));
            positions[2].set(-1.f,1.f,0.f,std::bit_cast<float>(1u));
            info.mVertexBuffer->setPositionData(positions.data());
        }
        info.mVertexBuffer->unmapBuffer();
        if(flavor.family==Inputs::ALScenePublicationNative::WorldFamily::legacy)
        {
            if(flavor.normal_map)info.mNormalMap.value=&image;
            if(flavor.specular_map)info.mSpecularMap.value=&image;
            if(indexed)
            {
                info.mMaterialSlotList.resize(2);
                for(auto& slot:info.mMaterialSlotList)
                {slot.mDiffuse.value=&image;if(flavor.normal_map)slot.mNormalMap.value=&image;if(flavor.specular_map)slot.mSpecularMap.value=&image;}
            }
        }
        else
        {
            for(U32 slot=0;slot<(indexed?2u:1u);++slot)
            {
                auto material=std::make_unique<Inputs::LLFetchedGLTFMaterial>();
                material->mAlphaMode=flavor.alpha_mask?Inputs::LLGLTFMaterial::ALPHA_MODE_MASK:Inputs::LLGLTFMaterial::ALPHA_MODE_OPAQUE;
                if(!slot)info.mGLTFMaterial.value=material.get();
                if(indexed)info.mGLTFMaterialList.push_back({material.get()});
                materials.push_back(std::move(material));
            }
        }
        return info;
    }
    std::shared_ptr<const NativeGroup> sealMaterial(ALNativeSourceWorkingSet& working_set=Inputs::sNativeSceneSources,U32 frame=Inputs::gFrameCount)
    {
        return Inputs::ALSceneGroupPublicationNative::sealMaterial(group,group_dependencies,group_layout,model,region.snapshot(),draws(),flavor,
            working_set,Inputs::sNativeSceneSerial,frame);
    }
    std::shared_ptr<const NativeGroup> currentMaterial(ALNativeSourceWorkingSet& working_set=Inputs::sNativeSceneSources,U32 frame=Inputs::gFrameCount)
    {return Inputs::ALSceneGroupPublicationNative::currentMaterial(group,region.snapshot(),flavor,working_set,frame);}
    void configure(Inputs::Capture& capture,bool compact=true,bool rows=true)
    {
        capture.compact_gbuffer_active=compact;capture.beginSourceRows(rows);
        capture.registerSourceModel(&model,region.snapshot());
        capture.material_normal_map=flavor.normal_map;capture.material_specular_map=flavor.specular_map;
        capture.material_alpha_mode=flavor.material_alpha_mode;capture.alpha_mask=flavor.alpha_mask;capture.pbr=type==Inputs::LLRenderPass::PASS_GLTF_PBR;
    }
    void capture(Inputs::LLPipeline& pipeline)
    {
        if(type==Inputs::LLRenderPass::PASS_MATERIAL)pipeline.captureMetalWorldMaterialPass(type);
        else pipeline.captureMetalWorldPBRPass(type);
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

TEST_CASE("Actual source native tuples reuse unchanged generations across CPU variants",
          "[almetal][pipeline-buffer-publication][native-tuple]")
{
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    auto first=Inputs::ALScenePublicationNative::world(info,flavor,Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(first);REQUIRE(first.owner->native_tuple);
    auto tuple=first.owner->native_tuple;
    CHECK((first.owner.owner_before(tuple) || tuple.owner_before(first.owner)));
    REQUIRE(tuple->resources.size()==first.owner->resources.size());
    for(std::size_t i=0;i<tuple->resources.size();++i)
    {
        CHECK(tuple->resources[i].allocation==first.owner->resources[i].allocation);
        CHECK(tuple->resources[i].bytes==first.owner->resources[i].bytes);
    }
    flavor.visibility_bounds=true;
    auto bounds=Inputs::ALScenePublicationNative::world(info,flavor,Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(bounds);
    CHECK(bounds.owner!=first.owner);CHECK(bounds.owner->native_tuple==tuple);
    CHECK(bounds.owner->bounds.qualified);CHECK_FALSE(first.owner->bounds.qualified);
}

TEST_CASE("Actual source residency callbacks distinguish cache withdrawal from producer mutation",
          "[almetal][pipeline-buffer-publication][scene-publication][native-tuple]")
{
    struct Reenter final : ALNativePublication::Observer
    {
        std::function<void()> action;bool fired=false,failed=false;
        void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
        {
            if(phase!=ALNativePublication::Phase::withdrawn || fired)return;
            fired=true;try{action();}catch(...){failed=true;}
        }
    };
    Publication f;
    for(bool mutate:{false,true})
    {
        CAPTURE(mutate);Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
        ALNativeSourceWorkingSet bounded(1,1024*1024,100);
        auto victim=std::make_shared<ALNativePublication::State>();auto value=std::make_shared<int>(1);
        REQUIRE(victim->publish(victim->revision(),value));REQUIRE(bounded.retain(victim,1,value,{},1));
        Inputs::ALScenePublicationNative::WorldFlavor flavor;
        Inputs::ALScenePublicationNative::WorldLease replacement;
        std::shared_ptr<const Inputs::ALScenePublicationNative::Payload> old_reader;
        auto observer=std::make_shared<Reenter>();
        observer->action=[&]
        {
            old_reader=std::static_pointer_cast<const Inputs::ALScenePublicationNative::Payload>(info.state->publication());
            if(mutate)
            {
                info.state->invalidate();
                replacement=Inputs::ALScenePublicationNative::world(info,flavor,bounded,Inputs::sNativeSceneSerial,2);
            }
            else info.state->evict();
        };
        victim->observe(observer);const auto revision=info.state->revision();
        auto outer=Inputs::ALScenePublicationNative::world(info,flavor,bounded,Inputs::sNativeSceneSerial,2);
        CHECK(observer->fired);CHECK_FALSE(observer->failed);REQUIRE(old_reader);REQUIRE(old_reader->native_tuple);
        if(mutate)
        {
            CHECK_FALSE(outer);REQUIRE(replacement);CHECK(info.state->revision()!=revision);
            CHECK(info.state->publication().get()==replacement.owner.get());
            CHECK(info.mMetalShadowSource->native_scene_revision==replacement.owner->producer_revision);
            CHECK(info.mMetalShadowSource->native_scene_transient.lock().get()==replacement.owner.get());
            CHECK(bounded.entryCount()==1);
        }
        else
        {
            REQUIRE(outer);CHECK(info.state->revision()==revision);CHECK_FALSE(info.state->publication());
            CHECK(outer.owner==old_reader);CHECK(bounded.entryCount()==0);
            auto transient=Inputs::ALScenePublicationNative::world(info,flavor,bounded,Inputs::sNativeSceneSerial,2);
            REQUIRE(transient);CHECK(transient.reused);CHECK(transient.owner==outer.owner);
        }
        CHECK(f.copied<F32>(*old_reader->vertices,0,4)[0]==-1.f);
    }
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
        cull.clear();pipeline.mMetalWorldSimpleCapture->clearDraws();
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
    auto packet=Inputs::ALSceneGroupPublicationNative::seal(f.group,f.group_dependencies,f.group_layout,f.model,members,
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
    CHECK(f.group_dependencies==old_dependencies);old_dependencies.reset(); // The owner relay is stable across row updates.
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
    scene={};cull.clear();packet.reset();capture.clearDraws();
    capture.world_resources=std::make_shared<Inputs::WorldResources>();
    f.group->invalidate();info.state->invalidate();gGL.frameFences().release();
    CHECK(weak.expired()); // CPU description is gone; the command retains only its NativeTuple.
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

TEST_CASE("Actual compact source and group descriptions retire before native command generations",
          "[almetal][pipeline-buffer-publication][native-tuple][compact-capture]")
{
    struct Observe final : ALNativePublication::Observer
    {
        U64 calls=0;
        void nativePublicationChanged(ALNativePublication::Phase) noexcept override{++calls;}
    };
    for(bool grouped:{false,true})for(bool prepared_path:{false,true})
    {
        CAPTURE(grouped,prepared_path);GroupPublication f;auto& info=f.draw();
        auto buffer=info.mVertexBuffer;
        auto observer=std::make_shared<Observe>();info.state->observe(observer);
        std::weak_ptr<ALNativePublication::State> weak_state=info.state;
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
        capture.compact_gbuffer_active=true;
        Inputs::LLCullResult cull;
        std::shared_ptr<const NativeGroup> group;
        std::weak_ptr<const Inputs::ALSceneGroupPublicationNative::Source> weak_group_row;
        std::shared_ptr<const void> cpu_description;
        std::shared_ptr<const Inputs::ALScenePublicationNative::NativeTuple> tuple;
        if(grouped)
        {
            group=f.seal();REQUIRE(group);tuple=group->native_tuple;cpu_description=group;
            weak_group_row=group->sources[0].owner;
            REQUIRE(cull.pushNativeSimpleRange(group,1));
            {CullScope scope(cull);pipeline.captureMetalWorldSimplePass();}
        }
        else
        {
            pipeline.captureMetalWorldGBufferBatch(info,true);
            auto source=std::static_pointer_cast<const Inputs::ALScenePublicationNative::Payload>(info.state->publication());
            REQUIRE(source);tuple=source->native_tuple;cpu_description=source;
        }
        REQUIRE(capture.complete);REQUIRE(tuple);REQUIRE(capture.source_references.size()==1);
        CHECK(capture.batches.empty());REQUIRE(capture.flushDrawOrder());
        CHECK((cpu_description.owner_before(tuple) || tuple.owner_before(cpu_description)));
        std::weak_ptr<const void> weak_description=cpu_description;
        std::weak_ptr<const Inputs::ALScenePublicationNative::NativeTuple> weak_tuple=tuple;
        const auto* original=capture.source_references[0].source->vertices;
        const auto position_offset=capture.source_references[0].source->position_offset;
        auto scene=f.scene(capture);std::vector<ALMetalWorldVisibilityQuery> queries;
        auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),
            std::move(capture.source_references),std::move(capture.draw_order),std::move(queries),
            std::move(capture.cpu_sources),capture.world_resources,1,&f.error);INFO(f.error);REQUIRE(inputs);
        scene={};capture.world_resources=std::make_shared<Inputs::WorldResources>();
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        REQUIRE(renderer->beginCommandBatch("Compact tuple independent retirement",&f.error));
        std::unique_ptr<ALMetalRenderTarget> target;
        if(prepared_path)
        {
            auto work=ALMetalWorldPreparation::create(inputs,1,{},&f.error);REQUIRE(work);REQUIRE(work->runJob(0));
            auto ready=work->finish(&f.error);INFO(f.error);REQUIRE(ready);
            target=renderer->renderPrepared(*ready,&f.error);
        }
        else target=renderer->renderOwnedInputs(*inputs,&f.error);
        INFO(f.error);REQUIRE(target);
        auto completion=renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
        REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,capture.draw_order,queries));
        capture.clearDraws();cull.clear();group.reset();cpu_description.reset();tuple.reset();
        f.group->invalidate();f.group_layout.reset();f.group_dependencies.reset();
        for(const auto& source:f.infos)source->state->invalidate();
        Inputs::sNativeSceneSources.clear();f.infos.clear();
        CHECK(weak_description.expired());CHECK(weak_state.expired());CHECK_FALSE(weak_tuple.expired());
        if(grouped)CHECK(weak_group_row.expired());
        const auto owner_calls=observer->calls;
        gGL.frameFences().release();const auto before=LLVertexBuffer::metalUploadStats();
        f.positions(*buffer,true);buffer->unmapBuffer();
        CHECK(buffer->getMetalVertexBuffer()!=original);
        CHECK(LLVertexBuffer::metalUploadStats().leased_orphans==before.leased_orphans+1);
        CHECK(f.copied<F32>(*original,position_offset,4)[0]==-1.f);
        REQUIRE(renderer->commitCommandBatch(&f.error));REQUIRE(f.device->waitUntilIdle(&f.error));
        CHECK(completion->isComplete());
        const auto pixels=f.read(*target);const auto pixel=(4*Publication::side+2)*4;
        CHECK(pixels.colors[0][pixel]==255);CHECK(pixels.colors[0][pixel+1]==0);
        renderer.reset();
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!weak_tuple.expired() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(weak_tuple.expired());CHECK(observer->calls==owner_calls);
    }
}

TEST_CASE("Actual compact CPU lease collisions retain nonadjacent sources through pending and abandoned work",
          "[almetal][pipeline-buffer-publication][compact-capture][cpu-source-lease]")
{
    for(bool pending:{false,true})
    {
        CAPTURE(pending);GroupPublication f;Inputs::LLPipeline pipeline;
        auto& capture=*pipeline.mMetalWorldSimpleCapture;capture.compact_gbuffer_active=true;
        std::array<Inputs::LLDrawInfo*,17> sources{};
        std::array<std::weak_ptr<const void>,17> descriptions;
        std::array<int,16> slots;slots.fill(-1);
        int first=-1,second=-1;
        for(std::size_t index=0;index<sources.size();++index)
        {
            sources[index]=&f.draw();pipeline.captureMetalWorldGBufferBatch(*sources[index],true);
            REQUIRE(capture.complete);const auto& owner=sources[index]->state->publication();REQUIRE(owner);
            descriptions[index]=owner;
            const auto bits=reinterpret_cast<std::uintptr_t>(owner.get());
            const auto slot=((bits>>4)^(bits>>12))&15;
            if(slots[slot]>=0 && first<0){first=slots[slot];second=static_cast<int>(index);}
            slots[slot]=static_cast<int>(index);
        }
        // Seventeen actual source publications necessarily collide in the
        // sixteen-slot production cache. Revisit the colliders with another
        // source between them, rather than testing consecutive duplicate hits.
        REQUIRE(first>=0);REQUIRE(second>=0);int separator=0;
        while(separator==first || separator==second)++separator;
        const auto before=capture.cpu_sources->owners.size();
        for(const int index:{first,separator,second,separator,first})
            pipeline.captureMetalWorldGBufferBatch(*sources[index],true);
        REQUIRE(capture.complete);CHECK(capture.logicalDrawCount()==22);
        CHECK(capture.cpu_sources->owners.size()>=before+2);
        const auto duplicate_pins=capture.cpu_sources->owners.size();
        pipeline.captureMetalWorldGBufferBatch(*sources[first],true);
        CHECK(capture.cpu_sources->owners.size()==duplicate_pins);
        for(auto* source:sources)source->state->invalidate();
        Inputs::sNativeSceneSources.clear();
        for(const auto& description:descriptions)CHECK_FALSE(description.expired());
        if(pending)
        {
            REQUIRE(capture.flushDrawOrder());auto scene=f.scene(capture);
            std::vector<ALMetalWorldVisibilityQuery> queries;
            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),
                std::move(capture.source_references),std::move(capture.draw_order),std::move(queries),
                std::move(capture.cpu_sources),capture.world_resources,1,&f.error);INFO(f.error);REQUIRE(inputs);
            scene={};auto work=ALMetalWorldPreparation::create(inputs,1,{},&f.error);INFO(f.error);REQUIRE(work);
            CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,
                capture.source_references,capture.draw_order,queries));
            inputs.reset();capture.clearDraws();
            for(const auto& description:descriptions)CHECK_FALSE(description.expired());
            work->cancel();work.reset();
        }
        else capture.clearDraws();
        // The native-only tuple aggregate may remain alive; abandoning the
        // last CPU reader still releases every description and cache address.
        REQUIRE(capture.world_resources);CHECK_FALSE(capture.world_resources->owners.empty());
        for(const auto& description:descriptions)CHECK(description.expired());
    }
}

TEST_CASE("Actual compact raw reference raw capture preserves logical queries and every attachment",
          "[almetal][pipeline-buffer-publication][compact-capture]")
{
    for(bool grouped:{false,true})
    {
        CAPTURE(grouped);GroupPublication f;
        auto& before=f.draw(LLColor4U(255,0,0,255));auto& middle=f.draw(LLColor4U(0,0,255,255));
        auto& after=f.draw(LLColor4U(255,255,0,255));f.positions(*middle.mVertexBuffer,true);middle.mVertexBuffer->unmapBuffer();
        Inputs::LLPipeline flat,compact;auto& capture=*compact.mMetalWorldSimpleCapture;
        capture.compact_gbuffer_active=true;
        for(auto* info:f.draws())flat.captureMetalWorldGBufferBatch(*info,true);
        compact.captureMetalWorldGBufferBatch(before,true,true); // Editable/bump-style raw boundary.
        if(grouped)
        {
            auto packet=Inputs::ALSceneGroupPublicationNative::seal(f.group,f.group_dependencies,f.group_layout,
                f.model,std::vector<Inputs::LLDrawInfo*>{&middle},Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,1);REQUIRE(packet);
            Inputs::LLCullResult cull;REQUIRE(cull.pushNativeSimpleRange(packet,1));
            CullScope scope(cull);compact.captureMetalWorldSimplePass();
        }
        else compact.captureMetalWorldGBufferBatch(middle,true);
        CHECK(capture.logicalDrawCount()==2); // Query issue point counts the reference too.
        compact.captureMetalWorldGBufferBatch(after,true,true);
        REQUIRE(capture.complete);REQUIRE(capture.flushDrawOrder());
        REQUIRE(capture.batches.size()==2);REQUIRE(capture.source_references.size()==1);REQUIRE(capture.draw_order.size()==3);
        CHECK(capture.draw_order[0]==0);CHECK(capture.draw_order[1]==alMetalWorldDrawReferenceBit);CHECK(capture.draw_order[2]==1);
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        std::array<ALMetalWorldVisibilityQuery,4> queries{};
        for(U32 index=0;index<queries.size();++index)
        {
            auto& query=queries[index];std::memcpy(query.modelview_projection,f.model.getF32ptr(),sizeof(query.modelview_projection));
            query.center[0]=.35f;query.center[1]=-.5f;query.center[2]=.75f;
            query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;
            query.batch_offset=index;query.fan=7;query.count_samples=1;
        }
        std::array<GroupPublication::Image,2> images;
        std::array<std::array<std::uint64_t,4>,2> counts{};
        for(U32 mode=0;mode<2;++mode)
        {
            auto scene=f.scene(mode?capture:*flat.mMetalWorldSimpleCapture);
            scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
            CHECK(scene.drawCount()==3);std::shared_ptr<ALMetalVisibilityResultBatch> result;
            auto target=renderer->render(scene,&f.error,&result);INFO(f.error);REQUIRE(target);REQUIRE(result);
            for(U32 query=0;query<queries.size();++query)REQUIRE(result->wait(query,counts[mode][query],&f.error));
            images[mode]=f.read(*target);
        }
        CHECK(images[0]==images[1]);CHECK(counts[0]==counts[1]);
        CHECK(counts[1][0]>0);CHECK(counts[1][1]>0);CHECK(counts[1][2]==0);CHECK(counts[1][3]==0);
        CHECK(images[1].colors[0][(4*Publication::side+10)*4+2]==255);
        CHECK(images[1].colors[0][(4*Publication::side+2)*4]==255);
        CHECK(images[1].colors[0][(4*Publication::side+2)*4+1]==255);
    }
}

TEST_CASE("Actual published compact families preserve masks and explicit raw consumer boundaries",
          "[almetal][pipeline-buffer-publication][compact-capture]")
{
    Publication f;
    for(U32 family=0;family<3;++family)for(bool masked:{false,true})
    {
        CAPTURE(family,masked);Inputs::LLDrawInfo info;
        info.mVertexBuffer=f.buffer(true,family==2?LLVertexBuffer::MAP_TANGENT:0);info.mTexture.value=&f.image;
        Inputs::LLFetchedGLTFMaterial material;
        if(family==2)
        {material.mAlphaMode=masked?Inputs::LLGLTFMaterial::ALPHA_MODE_MASK:Inputs::LLGLTFMaterial::ALPHA_MODE_OPAQUE;info.mGLTFMaterial.value=&material;}
        for(U32 boundary=0;boundary<5;++boundary)
        {
            CAPTURE(boundary);Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
            capture.compact_gbuffer_active=true;
            capture.alpha_mask=family!=1 && masked;
            capture.material_alpha_mode=family==1 && masked?2:0;
            capture.world_post_deferred_active=boundary==2;capture.impostor_bake=boundary==3;
            Inputs::gSavedSettings.native_visibility=boundary==4;
            pipeline.captureMetalPublishedWorldBatch(info,family,false,boundary==1);
            REQUIRE(capture.complete);
            if(boundary==0)
            {
                REQUIRE(capture.source_references.size()==1);CHECK(capture.batches.empty());
                const auto& source=*capture.source_references[0].source;
                CHECK(source.material_slot_count==(family==1?1:0));CHECK(source.pbr_slot_count==(family==2?1:0));
                CHECK(source.alpha_mask==(family!=1 && masked));CHECK(source.material_alpha_mode==(family==1 && masked?2:0));
            }
            else
            {
                REQUIRE(capture.batches.size()==1);CHECK(capture.source_references.empty());
                if(boundary==4)CHECK(capture.world_visibility_members.size()==1);
            }
        }
    }
}

TEST_CASE("Actual material cohorts bypass draw-info discovery and share each view matrix and owner",
          "[almetal][pipeline-buffer-publication][native-material-group]")
{
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);
        f.materialDraw();f.materialDraw(true);f.materialDraw();auto packet=f.sealMaterial();REQUIRE(packet);
        REQUIRE(packet->sources.size()==3);REQUIRE(packet->keyed_sources.size()==3);
        const auto reads=f.reads();const auto serial=Inputs::sNativeSceneSerial;
        for(U32 view=0;view<3;++view)
        {
            CAPTURE(view);REQUIRE(f.currentMaterial()==packet);Inputs::LLCullResult cull;
            REQUIRE(cull.pushNativeMaterialRange(f.type,packet,3,f.region.snapshot()));
            CHECK(cull.getRenderMapSize(f.type)==3);CHECK(cull.beginRenderMap(f.type)==cull.endRenderMap(f.type));
            Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;f.configure(capture);
            const auto matrices=Inputs::matrix_builds;{CullScope scope(cull);f.capture(pipeline);}
            REQUIRE(capture.complete);CHECK(Inputs::matrix_builds==matrices+1);
            CHECK(capture.source_references.empty());CHECK(capture.batches.empty());
            REQUIRE(capture.source_cohorts.size()==1);CHECK(capture.logicalDrawCount()==3);
            CHECK(capture.direct_cohort_appends==1);CHECK(capture.direct_cohort_draws==3);
            CHECK(capture.source_cohorts[0].cpu_owner.get()==packet.get());
            CHECK(capture.source_cohorts[0].native_owner.get()==packet->native_tuple.get());
            CHECK_FALSE(capture.cpu_sources);CHECK(capture.world_resources->owners.empty());
            CHECK_FALSE(capture.source_rows); // Required cohorts do not request the rejected GPU-row path.
            for(U32 row=0;row<3;++row)
            {
                CHECK(capture.source_cohorts[0].cohort->source(row).vertices==packet->sources[row].prototype->vertices);
                CHECK(capture.source_cohorts[0].cohort->source(row).index_offset==packet->sources[row].prototype->index_offset);
                CHECK(capture.source_cohorts[0].cohort->key(row).identity==packet->sources[row].owner->identity);
                CHECK_FALSE(packet->sources[row].owner->prototype.native_material_source);
                CHECK_FALSE(packet->sources[row].owner->prototype.native_resource_source);
            }
            CHECK(f.reads()==reads);CHECK(Inputs::sNativeSceneSerial==serial);
        }
        const auto geometry_packet=packet;const auto old_model=f.region.snapshot();
        REQUIRE(f.region.update(f.model,LLVector3(.25f,-.5f,0.f)));
        CHECK(f.currentMaterial()==geometry_packet);CHECK(f.region.snapshot().revision==old_model.revision+1);
        Inputs::LLCullResult rebased;REQUIRE(rebased.pushNativeMaterialRange(f.type,packet,3,f.region.snapshot()));
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;f.configure(capture);
        {CullScope scope(rebased);f.capture(pipeline);}REQUIRE(capture.complete);
        CHECK(capture.source_cohorts[0].view.modelview[12]==.25f);
        CHECK(capture.source_cohorts[0].view.modelview[13]==-.5f);
        CHECK(f.reads()==reads);CHECK(Inputs::sNativeSceneSerial==serial);
    }
}

TEST_CASE("Actual scalar and indexed material cohorts preserve every legacy family and PBR mask output",
          "[almetal][pipeline-buffer-publication][native-material-group]")
{
    const std::array<U32,12> masks{0,2,3,4,6,7,8,10,11,12,14,15};
    for(U32 family:{1u,2u})for(U32 index=0;index<(family==1?masks.size():2);++index)
    {
        const U32 mask=family==1?masks[index]:index;CAPTURE(family,mask);
        MaterialGroupPublication f(family,mask);f.materialDraw();f.materialDraw(true);
        auto packet=f.sealMaterial();REQUIRE(packet);
        Inputs::LLPipeline flat,grouped;f.configure(*flat.mMetalWorldSimpleCapture,true,false);
        f.configure(*grouped.mMetalWorldSimpleCapture,true,false);
        Inputs::LLCullResult raw,cohort;for(auto* info:f.draws())raw.pushDrawInfo(f.type,info);
        REQUIRE(cohort.pushNativeMaterialRange(f.type,packet,2,f.region.snapshot()));
        {CullScope scope(raw);f.capture(flat);}{CullScope scope(cohort);f.capture(grouped);}
        REQUIRE(flat.mMetalWorldSimpleCapture->complete);REQUIRE(grouped.mMetalWorldSimpleCapture->complete);
        const auto& a=flat.mMetalWorldSimpleCapture->source_references;
        const auto& b=grouped.mMetalWorldSimpleCapture->source_cohorts;
        REQUIRE(a.size()==2);REQUIRE(b.size()==1);
        CHECK(b[0].cohort->source(0).texture_count==1);CHECK(b[0].cohort->source(1).texture_count==2);
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        auto flat_target=renderer->render(f.scene(*flat.mMetalWorldSimpleCapture),&f.error);INFO(f.error);REQUIRE(flat_target);
        auto group_target=renderer->render(f.scene(*grouped.mMetalWorldSimpleCapture),&f.error);INFO(f.error);REQUIRE(group_target);
        CHECK(f.read(*flat_target)==f.read(*group_target));
    }
}

TEST_CASE("Actual material cull ranges preserve raw group raw logical order and query attachments",
          "[almetal][pipeline-buffer-publication][native-material-group]")
{
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);auto& before=f.materialDraw();
        auto& first=f.materialDraw(true);auto& second=f.materialDraw();auto& after=f.materialDraw();
        auto packet=Inputs::ALSceneGroupPublicationNative::sealMaterial(f.group,f.group_dependencies,f.group_layout,f.model,f.region.snapshot(),
            std::vector<Inputs::LLDrawInfo*>{&first,&second},f.flavor,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(packet);
        Inputs::LLCullResult raw,mixed;for(auto* info:f.draws())raw.pushDrawInfo(f.type,info);
        mixed.pushDrawInfo(f.type,&before);REQUIRE(mixed.pushNativeMaterialRange(f.type,packet,2,f.region.snapshot()));mixed.pushDrawInfo(f.type,&after);
        REQUIRE(mixed.nativeMaterialRanges(f.type).size()==1);CHECK(mixed.nativeMaterialRanges(f.type)[0].legacy_offset==1);
        Inputs::LLPipeline flat,grouped;f.configure(*flat.mMetalWorldSimpleCapture);f.configure(*grouped.mMetalWorldSimpleCapture);
        {CullScope scope(raw);f.capture(flat);}{CullScope scope(mixed);f.capture(grouped);}
        auto& capture=*grouped.mMetalWorldSimpleCapture;REQUIRE(capture.complete);REQUIRE(capture.flushDrawOrder());
        REQUIRE(capture.source_cohorts.size()==1);CHECK(capture.source_cohorts[0].logical_begin==1);
        CHECK(capture.source_cohorts[0].base_before==1);CHECK(capture.source_cohorts[0].cohort->size()==2);
        REQUIRE(capture.source_rows);REQUIRE(capture.source_rows->candidates.size()==2);
        CHECK(capture.source_rows->candidates[0].draw_index==0);CHECK(capture.source_rows->candidates[0].key.identity==before.state->identity());
        CHECK(capture.source_rows->candidates[1].draw_index==3);CHECK(capture.source_rows->candidates[1].key.identity==after.state->identity());
        std::array<ALMetalWorldVisibilityQuery,5> queries{};
        for(U32 index=0;index<queries.size();++index)
        {
            auto& query=queries[index];std::memcpy(query.modelview_projection,f.model.getF32ptr(),sizeof(query.modelview_projection));
            query.center[0]=.35f;query.center[1]=-.5f;query.center[2]=.75f;
            query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;query.batch_offset=index;query.fan=7;query.count_samples=1;
        }
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        std::array<GroupPublication::Image,2> images;std::array<std::array<std::uint64_t,5>,2> counts{};
        for(U32 mode=0;mode<2;++mode)
        {
            auto scene=f.scene(mode?capture:*flat.mMetalWorldSimpleCapture);scene.source_rows.reset();
            scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> result;auto target=renderer->render(scene,&f.error,&result);INFO(f.error);REQUIRE(target);REQUIRE(result);
            for(U32 query=0;query<queries.size();++query)REQUIRE(result->wait(query,counts[mode][query],&f.error));images[mode]=f.read(*target);
        }
        CHECK(images[0]==images[1]);CHECK(counts[0]==counts[1]);
    }
}

TEST_CASE("Actual material cohort churn restores weak cached rows without rediscovering unchanged draw infos",
          "[almetal][pipeline-buffer-publication][native-material-group]")
{
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);for(U32 row=0;row<4;++row)f.materialDraw(row%2);
        ALNativeSourceWorkingSet bounded(32,8*1024*1024,2);auto packet=f.sealMaterial(bounded,1);REQUIRE(packet);
        const auto old_rows=f.group_layout->sources;packet.reset();gGL.frameFences().release();
        REQUIRE(f.currentMaterial(bounded,4));
        auto unrelated=std::make_shared<ALNativePublication::State>();auto payload=std::make_shared<int>(1);
        REQUIRE(unrelated->publish(unrelated->revision(),payload));REQUIRE(bounded.retain(unrelated,900,payload,{},5));
        for(const auto& info:f.infos)CHECK_FALSE(info->state->publication());
        CHECK(f.group->publication());const auto before=f.reads();const auto before_builds=Inputs::ALSceneGroupPublicationNative::statistics.cpu_source_builds;
        const auto before_restores=Inputs::ALSceneGroupPublicationNative::statistics.material_restores;
        auto& dirty=*f.infos[1];
        // The raw getter records a new frame reader. Observe the retained
        // CPU layout's non-owning identity instead when testing cache-only
        // retirement; a genuine raw reader correctly requires orphaning.
        const auto* original=old_rows[1]->prototype.vertices;
        const auto uploads=LLVertexBuffer::metalUploadStats();
        const std::array<LLColor4U,3> green{LLColor4U(0,255,0,255),LLColor4U(0,255,0,255),LLColor4U(0,255,0,255)};
        dirty.mVertexBuffer->setColorData(green.data());dirty.mVertexBuffer->unmapBuffer();
        CHECK_FALSE(f.group->publication());CHECK(dirty.mVertexBuffer->getMetalVertexBuffer()==original);
        CHECK(LLVertexBuffer::metalUploadStats().leased_orphans==uploads.leased_orphans);
        auto refreshed=f.sealMaterial(bounded,5);REQUIRE(refreshed);const auto after=f.reads();
        CHECK(Inputs::ALSceneGroupPublicationNative::statistics.cpu_source_builds==before_builds+1);
        CHECK(Inputs::ALSceneGroupPublicationNative::statistics.material_restores==before_restores+3);
        CHECK(after[0]>before[0]);CHECK(after[1]==before[1]+2); // only the dirty source's actual factory dependency frontier.
        for(U32 row=0;row<4;++row)
        {if(row==1)CHECK(refreshed->sources[row].owner!=old_rows[row]);else CHECK(refreshed->sources[row].owner==old_rows[row]);}
        const auto stable=f.reads();Inputs::LLCullResult cull;REQUIRE(cull.pushNativeMaterialRange(f.type,refreshed,4,f.region.snapshot()));
        Inputs::LLPipeline pipeline;f.configure(*pipeline.mMetalWorldSimpleCapture);{CullScope scope(cull);f.capture(pipeline);}
        REQUIRE(pipeline.mMetalWorldSimpleCapture->complete);CHECK(f.reads()==stable);
    }
}

TEST_CASE("Actual material cohort admission rejects mutable scope and keeps whole publisher budgets",
          "[almetal][pipeline-buffer-publication][native-material-group]")
{
    for(U32 family:{1u,2u})for(U32 gate=0;gate<5;++gate)
    {
        CAPTURE(family,gate);MaterialGroupPublication f(family);auto& info=f.materialDraw();LLMatrix4 uv;uv.setIdentity();LLMatrix4a other=f.model;
        Inputs::LLVOAvatar avatar;int skin=0;
        if(gate==0)info.mTextureMatrix=&uv;if(gate==1)info.mModelMatrix=&other;
        if(gate==2)info.mAvatar=&avatar;if(gate==3)info.mSkinInfo=&skin;if(gate==4)f.flavor.visibility_bounds=true;
        CHECK_FALSE(f.sealMaterial());CHECK_FALSE(f.group->publication());
    }
    MaterialGroupPublication f;f.materialDraw();ALNativeSourceWorkingSet tiny(1,1,100);
    CHECK_FALSE(f.sealMaterial(tiny,1));CHECK_FALSE(f.group->publication());
    CHECK_FALSE(f.group_dependencies);CHECK_FALSE(f.group_layout);
}

TEST_CASE("Actual direct material cohort native resources outlive CPU descriptors and preserve an encoded old generation",
          "[almetal][pipeline-buffer-publication][native-material-group][native-tuple]")
{
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);auto& info=f.materialDraw(true);auto buffer=info.mVertexBuffer;
        auto packet=f.sealMaterial();REQUIRE(packet);std::weak_ptr<const NativeGroup> weak_group=packet;
        std::weak_ptr<const Inputs::ALScenePublicationNative::Payload> weak_source=packet->sources[0].world_owner;
        std::weak_ptr<ALNativePublication::State> weak_state=info.state;
        Inputs::LLCullResult cull;REQUIRE(cull.pushNativeMaterialRange(f.type,packet,1,f.region.snapshot()));
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;f.configure(capture,true,false);
        {CullScope scope(cull);f.capture(pipeline);}REQUIRE(capture.complete);
        REQUIRE(capture.source_cohorts.size()==1);
        const auto* original=capture.source_cohorts[0].cohort->source(0).vertices;
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        auto expected_target=renderer->render(f.scene(capture),&f.error);INFO(f.error);REQUIRE(expected_target);
        const auto expected_image=f.read(*expected_target);
        const auto cohort_before=renderer->worldCohortStats();
        const auto resources_before=renderer->worldResourceStats();
        REQUIRE(renderer->beginCommandBatch("Material cohort old native generation",&f.error));
        auto scene=f.scene(capture);scene.material_argument_buffers=true;
        auto target=renderer->render(scene,&f.error);INFO(f.error);REQUIRE(target);
        // PPD-193: this mechanism uses ordinary shaders, not argument tables.
        CHECK(renderer->worldCohortStats().draws==cohort_before.draws+1);
        CHECK(renderer->worldResourceStats().bound_draws==resources_before.bound_draws);
        auto completion=renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
        scene={};cull.clear();packet.reset();capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
        f.group->invalidate();f.group_layout.reset();f.group_dependencies.reset();
        for(const auto& source:f.infos)source->state->invalidate();
        Inputs::sNativeSceneSources.clear();f.infos.clear();
        CHECK(weak_group.expired());CHECK(weak_source.expired());CHECK(weak_state.expired());
        gGL.frameFences().release();const auto before=LLVertexBuffer::metalUploadStats();
        const std::array<LLColor4U,3> blue{LLColor4U(0,0,255,255),LLColor4U(0,0,255,255),LLColor4U(0,0,255,255)};
        buffer->setColorData(blue.data());buffer->unmapBuffer();
        CHECK(buffer->getMetalVertexBuffer()!=original);CHECK(LLVertexBuffer::metalUploadStats().leased_orphans==before.leased_orphans+1);
        REQUIRE(renderer->commitCommandBatch(&f.error));REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(completion->isComplete());
        CHECK(f.read(*target)==expected_image);
    }
}

TEST_CASE("Actual material cohort CPU admission rejection and reentrant replacement preserve exact publisher ownership",
          "[almetal][pipeline-buffer-publication][native-material-group][PPD-186]")
{
    using namespace Inputs::ALSceneGroupPublicationNative;
    struct RestorePolicy{CpuLayoutLimits before=cpu_layouts.limits();~RestorePolicy(){cpu_layouts.configure(before);}} restore;
    SECTION("post-publication CPU budget failure releases only the candidate")
    {
        MaterialGroupPublication f;f.materialDraw();auto limits=cpu_layouts.limits();
        limits.maximum_bytes=sizeof(Layout)+sizeof(NativeGroup)+sizeof(Source)+sizeof(std::shared_ptr<const Source>)+sizeof(SourceLease);
        cpu_layouts.configure(limits);REQUIRE(cpu_layouts.acceptsRows(1));
        CHECK_FALSE(f.sealMaterial());CHECK_FALSE(f.group->publication());CHECK(f.group->dirty());
        CHECK_FALSE(f.group_layout);CHECK_FALSE(f.group_dependencies);
    }
    SECTION("a newer callback publication wins over the stale outer candidate")
    {
        auto limits=cpu_layouts.limits();limits.maximum_layouts=1;cpu_layouts.configure(limits);
        auto victim=std::make_shared<ALNativePublication::State>();auto victim_layout=std::make_shared<Layout>();
        victim_layout->group_identity=victim->identity();auto victim_packet=std::make_shared<NativeGroup>();victim_packet->serial=90001;
        REQUIRE(victim->publish(victim->revision(),victim_packet));REQUIRE(cpu_layouts.retain(victim_layout,victim,*victim_packet,1));
        MaterialGroupPublication f;f.materialDraw();std::shared_ptr<const NativeGroup> newer,queued;
        struct Reenter final:ALNativePublication::Observer
        {
            std::function<void()> action;bool fired=false,failed=false;
            void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
            {if(phase!=ALNativePublication::Phase::withdrawn||fired)return;fired=true;try{action();}catch(...){failed=true;}}
        };
        auto observer=std::make_shared<Reenter>();observer->action=[&]
        {
            queued=std::static_pointer_cast<const NativeGroup>(f.group->publication());
            f.group->invalidate();newer=f.sealMaterial(Inputs::sNativeSceneSources,2);
        };
        victim->observe(observer);auto outer=f.sealMaterial(Inputs::sNativeSceneSources,2);
        CHECK(observer->fired);CHECK_FALSE(observer->failed);CHECK_FALSE(outer);REQUIRE(newer);REQUIRE(queued);
        CHECK(f.group->publication().get()==newer.get());CHECK(f.group_layout);CHECK(f.group_dependencies);
        CHECK(f.currentMaterial(Inputs::sNativeSceneSources,2)==newer);CHECK(queued->sources[0].prototype->vertices);
    }
}

TEST_CASE("Actual material pass null rows retain direct-probe failure and ordinary pool skip semantics",
          "[almetal][pipeline-buffer-publication][native-material-group][PPD-189]")
{
    for(U32 family:{1u,2u})for(bool direct:{false,true})
    {
        CAPTURE(family,direct);MaterialGroupPublication f(family);auto& info=f.materialDraw();
        Inputs::LLCullResult cull;cull.pushDrawInfo(f.type,&info);cull.pushDrawInfo(f.type,nullptr);
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;f.configure(capture);
        capture.direct_probe_scene_active=direct;{CullScope scope(cull);f.capture(pipeline);}
        CHECK(capture.complete==!direct);CHECK(capture.logicalDrawCount()==(direct?0:1));
        if(direct){CHECK_FALSE(capture.cpu_sources);CHECK_FALSE(capture.source_rows);}
    }
}

TEST_CASE("Actual direct material cohorts retain mixed raw reference order through adoption and reclamation",
          "[almetal][pipeline-buffer-publication][native-material-group][native-direct-cohort]")
{
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);
        auto& raw=f.materialDraw();auto& a=f.materialDraw();auto& b=f.materialDraw(true);auto& reference=f.materialDraw();
        const auto packet=Inputs::ALSceneGroupPublicationNative::sealMaterial(f.group,f.group_dependencies,f.group_layout,
            f.model,f.region.snapshot(),std::vector<Inputs::LLDrawInfo*>{&a,&b},f.flavor,
            Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(packet);REQUIRE(packet->cohort);
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;f.configure(capture,true,false);
        pipeline.captureMetalPublishedWorldBatch(raw,family,false,true);
        Inputs::LLCullResult cull;REQUIRE(cull.pushNativeMaterialRange(f.type,packet,2,f.region.snapshot()));
        {CullScope scope(cull);f.capture(pipeline);}
        pipeline.captureMetalPublishedWorldBatch(reference,family,false);
        REQUIRE(capture.complete);REQUIRE(capture.flushDrawOrder());CHECK(capture.logicalDrawCount()==4);
        REQUIRE(capture.batches.size()==1);REQUIRE(capture.source_references.size()==1);
        REQUIRE(capture.draw_order.size()==2);REQUIRE(capture.source_cohorts.size()==1);
        CHECK(capture.source_cohorts[0].logical_begin==1);CHECK(capture.source_cohorts[0].base_before==1);
        std::vector<ALMetalWorldVisibilityQuery> queries;
        auto inputs=ALMetalWorldViewInputs::adopt(f.scene(capture),std::move(capture.batches),
            std::move(capture.source_references),std::move(capture.draw_order),std::move(capture.source_cohorts),
            std::move(queries),std::move(capture.cpu_sources),capture.world_resources,Inputs::gFrameCount,&f.error);
        INFO(f.error);REQUIRE(inputs);CHECK(inputs->scene().drawCount()==4);
        CHECK(&inputs->scene().draw(1).source()==&packet->cohort->source(0));
        CHECK(&inputs->scene().draw(2).source()==&packet->cohort->source(1));
        CHECK(inputs->scene().draw(3).source().vertices==reference.mVertexBuffer->getMetalVertexBuffer());
        CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,
            capture.source_references,capture.draw_order,queries)); // Old API must not discard cohorts.
        REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,
            capture.draw_order,capture.source_cohorts,queries));
        CHECK(capture.source_cohorts.empty());capture.clearDraws();CHECK(capture.logicalDrawCount()==0);
        pipeline.captureMetalPublishedWorldBatch(raw,family,false,true);REQUIRE(capture.logicalDrawCount()==1);
        CHECK_FALSE(capture.appendCohort(packet->cohort,{},packet->native_tuple,{}));
        CHECK(capture.logicalDrawCount()==0);CHECK(capture.source_cohorts.empty());
    }
}

TEST_CASE("Actual material capabilities promote one semantic source without rebuilding viewer inputs",
          "[almetal][pipeline-buffer-publication][native-material-group][PPD-194]")
{
    using namespace Inputs::ALScenePublicationNative;
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);auto& info=f.materialDraw();
        auto ordinary=f.flavor;ordinary.native_resources=false;
        const auto records_before=cohort_source_publications;
        auto raw=world(info,ordinary,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(raw);
        CHECK_FALSE(raw.prototype->native_resource_source);CHECK(cohort_source_publications==records_before);
        const auto* original_variant=findWorldVariant(*raw.owner,ordinary.key());REQUIRE(original_variant);CHECK_FALSE(original_variant->cohort_source);
        const auto images=f.image.image_reads;const auto source_revision=info.state->revision();
        auto required=f.flavor;required.cohort_source=true;CHECK(required.key()==ordinary.key());
        auto promoted=world(info,required,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(promoted);
        REQUIRE(promoted.prototype->native_resource_source);REQUIRE(promoted.owner->worlds.size()==1);
        const auto* variant=findWorldVariant(*promoted.owner,required.key());REQUIRE(variant);REQUIRE(variant->cohort_source);
        CHECK(promoted.owner!=raw.owner);CHECK(raw.owner->worlds.size()==1);CHECK_FALSE(raw.prototype->native_resource_source);
        CHECK(promoted.owner->vertices==raw.owner->vertices);CHECK(promoted.owner->indices==raw.owner->indices);
        CHECK(promoted.owner->native_tuple==raw.owner->native_tuple);CHECK(info.state->revision()==source_revision);
        CHECK(f.image.image_reads==images);CHECK(cohort_source_publications==records_before+1);
        CHECK(variant->cohort_source->key().identity==info.state->identity());
        CHECK(variant->cohort_source->key().flavor==ordinary.key());CHECK(variant->cohort_source->key().revision==source_revision);
        const auto serial=Inputs::sNativeSceneSerial;
        auto reused=world(info,ordinary,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(reused);
        CHECK(reused.owner==promoted.owner);CHECK(Inputs::sNativeSceneSerial==serial);CHECK(cohort_source_publications==records_before+1);
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        // PPD-196: source publication is camera-independent. Apply the actual
        // fixture view to both snapshots, as ordinary capture does.
        auto original_draw=*raw.prototype,promoted_draw=*promoted.prototype;
        for(auto* draw:{&original_draw,&promoted_draw})
        {
            std::memcpy(draw->modelview_projection,f.model.getF32ptr(),sizeof(draw->modelview_projection));
            std::memcpy(draw->modelview,f.model.getF32ptr(),sizeof(draw->modelview));
            for(U32 axis=0;axis<3;++axis)
                draw->normal_matrix[axis][axis]=draw->rigged_normal_matrix[axis][axis]=1.f;
        }
        ALMetalWorldGBufferScene before;before.width=before.height=f.side;before.batches=&original_draw;before.batch_count=1;before.source_owner=raw.owner->native_tuple;
        auto after=before;after.batches=&promoted_draw;after.source_owner=promoted.owner->native_tuple;
        auto a=renderer->render(before,&f.error);INFO(f.error);REQUIRE(a);
        auto b=renderer->render(after,&f.error);INFO(f.error);REQUIRE(b);CHECK(f.read(*a)==f.read(*b));
    }
}

TEST_CASE("Actual partially dirty cohorts share immutable source proofs and refresh only changed generations",
          "[almetal][pipeline-buffer-publication][native-material-group][PPD-194]")
{
    using namespace Inputs::ALScenePublicationNative;
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);for(U32 row=0;row<4;++row)f.materialDraw(row%2);
        auto packet=f.sealMaterial();REQUIRE(packet);
        std::array<std::weak_ptr<const ALMetalWorldCohortSource>,4> records;
        for(U32 row=0;row<4;++row)
        {
            const auto* variant=findWorldVariant(*packet->sources[row].world_owner,packet->sources[row].owner->flavor_key);
            REQUIRE(variant);REQUIRE(variant->cohort_source);records[row]=variant->cohort_source;
        }
        packet.reset();const auto before=cohort_source_publications;
        const std::array<LLColor4U,3> green{LLColor4U(0,255,0,255),LLColor4U(0,255,0,255),LLColor4U(0,255,0,255)};
        auto& changed=*f.infos[1];changed.mVertexBuffer->setColorData(green.data());changed.mVertexBuffer->unmapBuffer();
        CHECK_FALSE(f.group->publication());CHECK(records[1].expired());
        auto refreshed=f.sealMaterial();REQUIRE(refreshed);CHECK(cohort_source_publications==before+1);
        for(U32 row=0;row<4;++row)
        {
            const auto* variant=findWorldVariant(*refreshed->sources[row].world_owner,refreshed->sources[row].owner->flavor_key);REQUIRE(variant);
            REQUIRE(variant->cohort_source);
            if(row==1)CHECK(variant->cohort_source->key().revision==changed.state->revision());
            else CHECK(variant->cohort_source==records[row].lock());
            CHECK(&refreshed->cohort->source(row)==&variant->cohort_source->source());
        }
    }
}

TEST_CASE("Actual optional material capability allocation failure preserves only the exact original publication",
          "[almetal][pipeline-buffer-publication][native-material-group][PPD-195]")
{
    using namespace Inputs::ALScenePublicationNative;
    struct RestoreHook {~RestoreHook(){capabilityPromotionAllocationForTesting=nullptr;}} restore;
    MaterialGroupPublication f;auto& info=f.materialDraw();auto ordinary=f.flavor;ordinary.native_resources=false;
    auto raw=world(info,ordinary,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(raw);
    auto required=f.flavor;required.cohort_source=true;const auto revision=info.state->revision();
    SECTION("an allocation failure before mutation retains the ordinary source")
    {
        capabilityPromotionAllocationForTesting=[](ALNativePublication::State&){throw std::bad_alloc();};
        auto result=world(info,required,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(result);
        CHECK(result.owner==raw.owner);CHECK(result.prototype==raw.prototype);CHECK_FALSE(result.prototype->native_resource_source);
        CHECK(info.state->publication().get()==raw.owner.get());CHECK(info.state->revision()==revision);
    }
    SECTION("producer mutation forbids the old source fallback")
    {
        capabilityPromotionAllocationForTesting=[](ALNativePublication::State& state){state.invalidate();throw std::bad_alloc();};
        CHECK_FALSE(world(info,required,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount));
        CHECK(info.state->revision()==revision+1);CHECK_FALSE(info.state->publication());
    }
    SECTION("unavailable capabilities also reject an invalidated producer")
    {
        required.native_resources=false;
        capabilityPromotionAllocationForTesting=[](ALNativePublication::State& state){state.invalidate();};
        CHECK_FALSE(world(info,required,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount));
        CHECK(info.state->revision()==revision+1);CHECK_FALSE(info.state->publication());
    }
    SECTION("a newer same-revision publication is never replaced by fallback")
    {
        capabilityPromotionAllocationForTesting=[](ALNativePublication::State& state)
        {
            auto old=std::static_pointer_cast<const Payload>(state.publication());
            auto replacement=std::make_shared<Payload>(*old);replacement->serial=old->serial+100;
            (void)state.publish(state.revision(),replacement);throw std::bad_alloc();
        };
        CHECK_FALSE(world(info,required,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount));
        REQUIRE(info.state->publication());CHECK(info.state->publication().get()!=raw.owner.get());
        CHECK(info.state->revision()==revision);
    }
    SECTION("a newer same-revision publication is never replaced by successful promotion")
    {
        capabilityPromotionAllocationForTesting=[](ALNativePublication::State& state)
        {
            auto old=std::static_pointer_cast<const Payload>(state.publication());
            auto replacement=std::make_shared<Payload>(*old);replacement->serial=old->serial+100;
            (void)state.publish(state.revision(),replacement);
        };
        CHECK_FALSE(world(info,required,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount));
        REQUIRE(info.state->publication());CHECK(info.state->publication().get()!=raw.owner.get());
        CHECK_FALSE(std::static_pointer_cast<const Payload>(info.state->publication())->worlds[0].cohort_source);
        CHECK(info.state->revision()==revision);
    }
}

TEST_CASE("Actual same-view material replacement preserves endpoint warm groups and rejects overlapping spans",
          "[almetal][pipeline-buffer-publication][native-material-group][native-same-view-cohort]")
{
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);
        auto& before=f.materialDraw();auto& warm_info=f.materialDraw();auto& a=f.materialDraw(true);auto& b=f.materialDraw();auto& after=f.materialDraw();
        struct Owner
        {
            std::shared_ptr<ALNativePublication::State> state=std::make_shared<ALNativePublication::State>();
            std::shared_ptr<ALNativePublication::Observer> dependencies;
            std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
            ~Owner(){state->invalidate();}
        } warm_owner,replacement_owner;
        const auto seal=[&](Owner& owner,const std::vector<Inputs::LLDrawInfo*>& sources)
        {return Inputs::ALSceneGroupPublicationNative::sealMaterial(owner.state,owner.dependencies,owner.layout,f.model,
            f.region.snapshot(),sources,f.flavor,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);};
        const auto warm=seal(warm_owner,{&warm_info});const auto packet=seal(replacement_owner,{&a,&b});REQUIRE(warm);REQUIRE(packet);
        Inputs::LLCullResult cull;cull.pushDrawInfo(f.type,&before);REQUIRE(cull.pushNativeMaterialRange(f.type,warm,1,f.region.snapshot()));
        cull.pushDrawInfo(f.type,&a);cull.pushDrawInfo(f.type,&b);REQUIRE(cull.pushNativeMaterialRange(f.type,warm,1,f.region.snapshot()));cull.pushDrawInfo(f.type,&after);
        const auto size=cull.getRenderMapSize(f.type);auto* raw_begin=cull.beginRenderMap(f.type);auto* raw_end=cull.endRenderMap(f.type);
        std::vector<Inputs::LLDrawInfo*> expected{&a,&b};
        REQUIRE(cull.replaceNativeMaterialRange(f.type,packet,1,expected,f.region.snapshot()));
        CHECK(cull.getRenderMapSize(f.type)==size);CHECK(cull.beginRenderMap(f.type)==raw_begin);CHECK(cull.endRenderMap(f.type)==raw_end);
        REQUIRE(cull.nativeMaterialRanges(f.type).size()==3);
        CHECK(cull.nativeMaterialRanges(f.type)[0].legacy_offset==1);CHECK(cull.nativeMaterialRanges(f.type)[0].raw_skip==0);
        CHECK(cull.nativeMaterialRanges(f.type)[1].legacy_offset==1);CHECK(cull.nativeMaterialRanges(f.type)[1].raw_skip==2);
        CHECK(cull.nativeMaterialRanges(f.type)[2].legacy_offset==3);CHECK(cull.nativeMaterialRanges(f.type)[2].raw_skip==0);
        CHECK_FALSE(cull.replaceNativeMaterialRange(f.type,packet,1,expected,f.region.snapshot()));
        const std::vector<Inputs::LLDrawInfo*> overlapping{&b,&after};
        CHECK_FALSE(cull.replaceNativeMaterialRange(f.type,packet,2,overlapping,f.region.snapshot()));
        const std::vector<Inputs::LLDrawInfo*> reversed{&b,&a};CHECK_FALSE(cull.replaceNativeMaterialRange(f.type,packet,1,reversed,f.region.snapshot()));
        Inputs::LLCullResult changed;changed.pushDrawInfo(f.type,&a);changed.pushDrawInfo(f.type,&b);
        CHECK_FALSE(changed.replaceNativeMaterialRange(f.type,packet,0,reversed,f.region.snapshot()));
        CHECK(changed.nativeMaterialRanges(f.type).empty());CHECK(changed.getRenderMapSize(f.type)==2);
        Inputs::LLPipeline grouped,flat;f.configure(*grouped.mMetalWorldSimpleCapture,false,false);f.configure(*flat.mMetalWorldSimpleCapture,false,false);
        {CullScope scope(cull);f.capture(grouped);}REQUIRE(grouped.mMetalWorldSimpleCapture->complete);
        auto scene=f.scene(*grouped.mMetalWorldSimpleCapture);REQUIRE(scene.drawCount()==6);
        const std::array<Inputs::LLDrawInfo*,6> order{&before,&warm_info,&a,&b,&warm_info,&after};
        for(U32 row=0;row<order.size();++row)CHECK(scene.draw(row).source().vertices==order[row]->mVertexBuffer->getMetalVertexBuffer());
        Inputs::LLCullResult raw;for(auto* info:order)raw.pushDrawInfo(f.type,info);{CullScope scope(raw);f.capture(flat);}
        std::array<ALMetalWorldVisibilityQuery,7> queries{};
        for(U32 query=0;query<queries.size();++query)
        {auto& q=queries[query];std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
         q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=.75f;q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;q.batch_offset=query;q.fan=7;q.count_samples=1;}
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);INFO(f.error);REQUIRE(renderer);
        std::array<GroupPublication::Image,2> images;std::array<std::array<std::uint64_t,7>,2> results{};
        for(U32 mode=0;mode<2;++mode)
        {auto view=mode?scene:f.scene(*flat.mMetalWorldSimpleCapture);view.visibility_queries=queries.data();view.visibility_query_count=queries.size();
         std::shared_ptr<ALMetalVisibilityResultBatch> counts;auto target=renderer->render(view,&f.error,&counts);INFO(f.error);REQUIRE(target);REQUIRE(counts);
         for(U32 query=0;query<queries.size();++query)REQUIRE(counts->wait(query,results[mode][query],&f.error));images[mode]=f.read(*target);}
        CHECK(images[0]==images[1]);CHECK(results[0]==results[1]);
        Inputs::LLCullResult inside;inside.pushDrawInfo(f.type,&a);REQUIRE(inside.pushNativeMaterialRange(f.type,warm,1,f.region.snapshot()));inside.pushDrawInfo(f.type,&b);
        CHECK_FALSE(inside.replaceNativeMaterialRange(f.type,packet,0,expected,f.region.snapshot()));
        CHECK(inside.getRenderMapSize(f.type)==3);CHECK(inside.nativeMaterialRanges(f.type).size()==1);
        CHECK(inside.beginRenderMap(f.type)[0]==&a);CHECK(inside.beginRenderMap(f.type)[1]==&b);
        Inputs::LLPipeline fallback;f.configure(*fallback.mMetalWorldSimpleCapture,false,false);
        {CullScope scope(inside);f.capture(fallback);}REQUIRE(fallback.mMetalWorldSimpleCapture->complete);
        auto unchanged=f.scene(*fallback.mMetalWorldSimpleCapture);REQUIRE(unchanged.drawCount()==3);
        CHECK(unchanged.draw(0).source().vertices==a.mVertexBuffer->getMetalVertexBuffer());
        CHECK(unchanged.draw(1).source().vertices==warm_info.mVertexBuffer->getMetalVertexBuffer());
        CHECK(unchanged.draw(2).source().vertices==b.mVertexBuffer->getMetalVertexBuffer());
    }
}

TEST_CASE("Actual source-row capture preserves regional order across typed main and probe views",
          "[almetal][pipeline-buffer-publication][native-source-rows]")
{
    GroupPublication f;std::array<ALNativeRegionModel,2> models;
    std::array<LLMatrix4a,2> matrices;
    REQUIRE(models[0].update(matrices[0],LLVector3(0.f,0.f,0.f)));
    REQUIRE(models[1].update(matrices[1],LLVector3(.25f,.5f,0.f)));
    auto& legacy=f.draw();legacy.mModelMatrix=&matrices[0];
    auto& pbr=f.draw();pbr.mVertexBuffer=f.buffer(true,LLVertexBuffer::MAP_TANGENT);pbr.mModelMatrix=&matrices[1];
    Inputs::LLFetchedGLTFMaterial material;pbr.mGLTFMaterial.value=&material;
    std::array<Inputs::LLDrawInfo*,2> infos{&legacy,&pbr};
    std::array<std::shared_ptr<const ALMetalWorldSourceView>,3> retained;
    const std::array<std::array<U32,3>,3> order{{{{0,1,0}},{{1,0,1}},{{0,0,1}}}};
    std::array<ALMetalWorldSourceKey,2> stable_keys{};
    // Each capture is one independent typed main/probe view. Camera/gGL is
    // still the fixture's declared identity-camera frontier in all three.
    for(U32 view=0;view<3;++view)
    {
        CAPTURE(view);Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
        capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
        capture.registerSourceModel(&matrices[1],models[1].snapshot());
        capture.registerSourceModel(&matrices[0],models[0].snapshot()); // registration order is not draw order.
        for(U32 source:order[view])pipeline.captureMetalPublishedWorldBatch(*infos[source],source?2:1,false);
        REQUIRE(capture.complete);REQUIRE(capture.source_rows);
        REQUIRE(capture.source_rows->candidates.size()==3);REQUIRE(capture.source_rows->regions.size()==2);
        REQUIRE(capture.source_references.size()==3);CHECK(capture.batches.empty());
        std::array<int,2> first_region{-1,-1};
        for(U32 draw=0;draw<3;++draw)
        {
            const auto source=order[view][draw];const auto& candidate=capture.source_rows->candidates[draw];
            CHECK(candidate.draw_index==draw);CHECK(candidate.key.identity==infos[source]->state->identity());
            CHECK(candidate.key.revision==infos[source]->state->revision());
            if(first_region[source]<0)first_region[source]=static_cast<int>(candidate.region);
            CHECK(candidate.region==static_cast<U32>(first_region[source]));
            const auto& region=capture.source_rows->regions[candidate.region];
            const auto& matrices_for_draw=capture.source_references[draw].view;
            CHECK(std::memcmp(region.modelview_projection,matrices_for_draw.modelview_projection,sizeof(region.modelview_projection))==0);
            CHECK(std::memcmp(region.modelview,matrices_for_draw.modelview,sizeof(region.modelview))==0);
            CHECK(std::memcmp(region.normal_matrix,matrices_for_draw.normal_matrix,sizeof(region.normal_matrix))==0);
            if(view==0)stable_keys[source]=candidate.key;
            else
            {CHECK(candidate.key.identity==stable_keys[source].identity);CHECK(candidate.key.flavor==stable_keys[source].flavor);CHECK(candidate.key.revision==stable_keys[source].revision);}
        }
        CHECK(capture.source_rows->candidates[0].region==0);
        retained[view]=capture.source_rows;capture.clearDraws();
        CHECK_FALSE(capture.source_rows);CHECK_FALSE(capture.source_rows_active);
        CHECK(capture.source_model_pointers.empty());
    }
    CHECK(retained[0]->regions[0].modelview[12]==0.f);
    CHECK(retained[1]->regions[0].modelview[12]==.25f);
    CHECK(retained[2]->regions[0].modelview[12]==0.f);
}

TEST_CASE("Actual source-row metadata preserves raw reference boundaries and copied model generations",
          "[almetal][pipeline-buffer-publication][native-source-rows]")
{
    for(bool compact:{false,true})
    {
        CAPTURE(compact);GroupPublication f;ALNativeRegionModel model;LLMatrix4a matrix;
        REQUIRE(model.update(matrix,LLVector3(0.f,0.f,0.f)));
        auto& info=f.draw();info.mModelMatrix=&matrix;
        auto& simple=f.draw();simple.mModelMatrix=&matrix;
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
        capture.beginSourceRows(true);capture.compact_gbuffer_active=compact;
        capture.registerSourceModel(&matrix,model.snapshot());
        pipeline.captureMetalWorldGBufferBatch(simple,true,true); // raw editable/simple gap.
        pipeline.captureMetalPublishedWorldBatch(info,1,false);
        pipeline.captureMetalPublishedWorldBatch(info,1,false,true); // raw editable legacy gap.
        pipeline.captureMetalPublishedWorldBatch(info,1,false);
        REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==4);REQUIRE(capture.source_rows);
        REQUIRE(capture.source_rows->candidates.size()==2);REQUIRE(capture.source_rows->regions.size()==1);
        CHECK(capture.source_rows->candidates[0].draw_index==1);CHECK(capture.source_rows->candidates[1].draw_index==3);
        if(compact)
        {
            REQUIRE(capture.flushDrawOrder());CHECK(capture.batches.size()==2);CHECK(capture.source_references.size()==2);
            REQUIRE(capture.draw_order.size()==4);CHECK(capture.draw_order[0]==0);
            CHECK(capture.draw_order[1]==alMetalWorldDrawReferenceBit);CHECK(capture.draw_order[2]==1);
            CHECK(capture.draw_order[3]==(alMetalWorldDrawReferenceBit|1));
        }
        else {CHECK(capture.batches.size()==4);CHECK(capture.source_references.empty());}
        const auto first=capture.source_rows;
        const auto key=first->candidates[0].key;
        const auto serial=std::static_pointer_cast<const Inputs::ALScenePublicationNative::Payload>(info.state->publication())->serial;
        std::weak_ptr<const void> description=info.state->publication();
        REQUIRE(model.update(matrix,LLVector3(.5f,-.25f,0.f)));
        CHECK(info.state->revision()==key.revision); // model epochs do not dirty geometry/material sources.
        capture.clearDraws();capture.beginSourceRows(true);capture.compact_gbuffer_active=compact;
        capture.registerSourceModel(&matrix,model.snapshot());pipeline.captureMetalPublishedWorldBatch(info,1,false);
        REQUIRE(capture.complete);REQUIRE(capture.source_rows);REQUIRE(capture.source_rows->candidates.size()==1);
        const auto& next=capture.source_rows->candidates[0].key;
        CHECK(next.identity==key.identity);CHECK(next.flavor==key.flavor);CHECK(next.revision==key.revision);
        CHECK(std::static_pointer_cast<const Inputs::ALScenePublicationNative::Payload>(info.state->publication())->serial==serial);
        CHECK(first->regions[0].modelview[12]==0.f);CHECK(capture.source_rows->regions[0].modelview[12]==.5f);
        CHECK(capture.source_rows->regions[0].modelview[13]==-.25f);
        auto retained=capture.source_rows;capture.clearDraws();info.state->invalidate();simple.state->invalidate();
        Inputs::sNativeSceneSources.clear();model.withdraw();
        CHECK(description.expired());CHECK(retained->candidates[0].key.identity==key.identity);
        CHECK(retained->regions[0].modelview[12]==.5f); // metadata retains no source or model owner.
    }
}

TEST_CASE("Actual source-row admission rejects pointer impostors stale models and animated UV without losing draws",
          "[almetal][pipeline-buffer-publication][native-source-rows]")
{
    GroupPublication f;ALNativeRegionModel model;LLMatrix4a matrix;
    REQUIRE(model.update(matrix,LLVector3(0.f,0.f,0.f)));const auto old=model.snapshot();
    LLMatrix4a byte_equal=matrix;LLMatrix4 uv;uv.setIdentity();
    for(U32 gate=0;gate<5;++gate)
    {
        CAPTURE(gate);auto& info=f.draw();info.mModelMatrix=gate==0?&byte_equal:&matrix;
        info.mTextureMatrix=gate==2?&uv:nullptr;
        Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
        capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
        if(gate==1)
        {REQUIRE(model.update(matrix,LLVector3(.25f,0.f,0.f)));capture.registerSourceModel(&matrix,old);}
        else if(gate==3)capture.registerSourceModel(&matrix,ALNativeRegionModel::Snapshot{});
        else capture.registerSourceModel(&matrix,model.snapshot());
        pipeline.captureMetalPublishedWorldBatch(info,1,false,gate==4);
        REQUIRE(capture.complete);CHECK(capture.logicalDrawCount()==1);
        CHECK_FALSE(capture.source_rows); // equality of matrix bytes cannot confer region ownership.
    }
}

TEST_CASE("Actual source-row model re-registration withdraws stale owner proofs within one capture",
          "[almetal][pipeline-buffer-publication][native-source-rows][publication-edge]")
{
    for(bool retire:{false,true})
    {
        CAPTURE(retire);GroupPublication f;ALNativeRegionModel model;LLMatrix4a matrix;
        REQUIRE(model.update(matrix,LLVector3(0.f,0.f,0.f)));const auto first=model.snapshot();
        auto& info=f.draw();info.mModelMatrix=&matrix;Inputs::LLPipeline pipeline;
        auto& capture=*pipeline.mMetalWorldSimpleCapture;
        capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
        capture.registerSourceModel(&matrix,first);pipeline.captureMetalPublishedWorldBatch(info,1,false);
        REQUIRE(capture.complete);REQUIRE(capture.source_rows);REQUIRE(capture.source_rows->candidates.size()==1);
        const auto original_region=capture.source_rows->regions[0];
        const auto key=capture.source_rows->candidates[0].key;
        if(retire)model.withdraw();else REQUIRE(model.update(matrix,LLVector3(.5f,0.f,0.f)));
        capture.registerSourceModel(&matrix,retire?model.snapshot():first);
        CHECK(capture.source_model_pointers.empty());
        pipeline.captureMetalPublishedWorldBatch(info,1,false);
        REQUIRE(capture.complete);CHECK(capture.logicalDrawCount()==2);
        REQUIRE(capture.source_rows);CHECK(capture.source_rows->candidates.size()==1);
        CHECK(std::memcmp(&capture.source_rows->regions[0],&original_region,sizeof(original_region))==0);
        if(!retire)
        {
            capture.registerSourceModel(&matrix,model.snapshot());pipeline.captureMetalPublishedWorldBatch(info,1,false);
            REQUIRE(capture.complete);CHECK(capture.logicalDrawCount()==3);
            REQUIRE(capture.source_rows->candidates.size()==2);CHECK(capture.source_rows->candidates[1].draw_index==2);
            CHECK(capture.source_rows->candidates[1].key.identity==key.identity);
            CHECK(capture.source_rows->candidates[1].key.revision==key.revision);
            REQUIRE(capture.source_rows->regions.size()==2);CHECK(capture.source_rows->regions[1].modelview[12]==.5f);
            CHECK(std::memcmp(&capture.source_rows->regions[0],&original_region,sizeof(original_region))==0);
        }
    }
}

TEST_CASE("Actual source-row optional metadata is bounded and fails without a missing required draw",
          "[almetal][pipeline-buffer-publication][native-source-rows]")
{
    GroupPublication f;ALNativeRegionModel model;LLMatrix4a matrix;
    REQUIRE(model.update(matrix,LLVector3(0.f,0.f,0.f)));auto& info=f.draw();info.mModelMatrix=&matrix;
    Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
    capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
    capture.registerSourceModel(&matrix,model.snapshot());
    for(std::size_t draw=0;draw<alMetalWorldSourceMaximumRows;++draw)
        pipeline.captureMetalPublishedWorldBatch(info,1,false);
    REQUIRE(capture.complete);REQUIRE(capture.source_rows);
    CHECK(capture.source_rows->candidates.size()==alMetalWorldSourceMaximumRows);
    CHECK(capture.source_rows->regions.size()==1);
    pipeline.captureMetalPublishedWorldBatch(info,1,false);
    CHECK(capture.complete);CHECK(capture.logicalDrawCount()==alMetalWorldSourceMaximumRows+1);
    CHECK_FALSE(capture.source_rows);CHECK_FALSE(capture.source_rows_active);
    CHECK(capture.source_rows_stats.requested_ordinary_rows==alMetalWorldSourceMaximumRows+1);
    CHECK(capture.source_rows_stats.emitted_candidates==alMetalWorldSourceMaximumRows);
    CHECK(capture.source_rows_stats.optional_metadata_failures==1);

    capture.clearDraws();capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
    std::vector<LLMatrix4a> matrices(alMetalWorldSourceMaximumRows+1);
    std::vector<std::unique_ptr<ALNativeRegionModel>> models;
    for(auto& current:matrices)
    {
        auto native=std::make_unique<ALNativeRegionModel>();REQUIRE(native->update(current,LLVector3(0.f,0.f,0.f)));
        capture.registerSourceModel(&current,native->snapshot());models.push_back(std::move(native));
    }
    CHECK_FALSE(capture.source_rows_active);CHECK(capture.source_model_pointers.empty());
    CHECK(capture.source_rows_stats.enabled_views==2);
    CHECK(capture.source_rows_stats.registered_models==alMetalWorldSourceMaximumRows+1);
    CHECK(capture.source_rows_stats.optional_metadata_failures==2);
    pipeline.captureMetalPublishedWorldBatch(info,1,false);
    CHECK(capture.complete);CHECK(capture.logicalDrawCount()==1);CHECK_FALSE(capture.source_rows);
    CHECK(capture.source_rows_stats.requested_ordinary_rows==alMetalWorldSourceMaximumRows+1);
}

TEST_CASE("Actual source-row capture telemetry distinguishes unsupported sources owner proofs and use",
          "[almetal][pipeline-buffer-publication][native-source-rows]")
{
    GroupPublication f;ALNativeRegionModel model;LLMatrix4a matrix;
    REQUIRE(model.update(matrix,LLVector3(0.f,0.f,0.f)));LLMatrix4a impostor=matrix;
    auto& simple=f.draw();simple.mModelMatrix=&matrix;
    auto& wrong_owner=f.draw();wrong_owner.mModelMatrix=&impostor;
    auto& animated=f.draw();animated.mModelMatrix=&matrix;LLMatrix4 uv;uv.setIdentity();animated.mTextureMatrix=&uv;
    auto& valid=f.draw();valid.mModelMatrix=&matrix;
    Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
    capture.beginSourceRows(false);CHECK(capture.source_rows_stats.enabled_views==0);
    capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
    capture.registerSourceModel(&matrix,model.snapshot());capture.registerSourceModel(&matrix,model.snapshot());
    pipeline.captureMetalWorldGBufferBatch(simple,true);
    pipeline.captureMetalPublishedWorldBatch(wrong_owner,1,false);
    pipeline.captureMetalPublishedWorldBatch(animated,1,false);
    pipeline.captureMetalPublishedWorldBatch(valid,1,false);
    REQUIRE(capture.complete);CHECK(capture.logicalDrawCount()==4);
    CHECK(capture.source_rows_stats.enabled_views==1);CHECK(capture.source_rows_stats.registered_models==1);
    CHECK(capture.source_rows_stats.requested_ordinary_rows==4);CHECK(capture.source_rows_stats.emitted_candidates==1);
    CHECK(capture.source_rows_stats.unsupported_skips==2);CHECK(capture.source_rows_stats.owner_proof_skips==1);
    CHECK(capture.source_rows_stats.optional_metadata_failures==0);
    capture.clearDraws();CHECK_FALSE(capture.source_rows);CHECK_FALSE(capture.source_rows_active);
    CHECK(capture.source_rows_stats.requested_ordinary_rows==4);CHECK(capture.source_rows_stats.emitted_candidates==1);
    capture.beginSourceRows(true);capture.compact_gbuffer_active=true;
    capture.registerSourceModel(&matrix,model.snapshot());pipeline.captureMetalPublishedWorldBatch(valid,1,false);
    REQUIRE(capture.complete);CHECK(capture.logicalDrawCount()==1);
    CHECK(capture.source_rows_stats.enabled_views==2);CHECK(capture.source_rows_stats.registered_models==2);
    CHECK(capture.source_rows_stats.requested_ordinary_rows==5);CHECK(capture.source_rows_stats.emitted_candidates==2);
    CHECK(capture.source_rows_stats.unsupported_skips==2);CHECK(capture.source_rows_stats.owner_proof_skips==1);
    CHECK(capture.source_rows_stats.optional_metadata_failures==0);
}

TEST_CASE("Actual source-row regional reuse compares all copied view bytes",
          "[almetal][pipeline-buffer-publication][native-source-rows]")
{
    GroupPublication f;ALNativeRegionModel model;LLMatrix4a matrix;
    REQUIRE(model.update(matrix,LLVector3(0.f,0.f,0.f)));auto& info=f.draw();info.mModelMatrix=&matrix;
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    flavor.family=Inputs::ALScenePublicationNative::WorldFamily::legacy;flavor.native_resources=true;
    auto source=Inputs::ALScenePublicationNative::world(info,flavor,Inputs::sNativeSceneSources,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(source);REQUIRE(source.prototype->native_resource_source);
    Inputs::Capture capture;capture.beginSourceRows(true);capture.registerSourceModel(&matrix,model.snapshot());
    ALMetalWorldViewMatrices first;
    std::copy_n(matrix.getF32ptr(),16,first.modelview);std::copy_n(matrix.getF32ptr(),16,first.modelview_projection);
    for(U32 axis=0;axis<3;++axis)first.normal_matrix[axis][axis]=1.f;
    auto capture_view=[&](const ALMetalWorldViewMatrices& view)
    {capture.captureSourceRow(info,source,flavor,view,true);REQUIRE(capture.appendReference(*source.prototype,view));};
    capture_view(first);capture_view(first);
    auto other=first;other.normal_matrix[1][2]=.125f;capture_view(other);
    other=first;other.modelview_projection[12]=.25f;capture_view(other);
    other=first;other.modelview[13]=.5f;capture_view(other);
    REQUIRE(capture.source_rows);REQUIRE(capture.source_rows->regions.size()==4);
    const auto& rows=capture.source_rows->candidates;REQUIRE(rows.size()==5);
    CHECK(rows[0].region==0);CHECK(rows[1].region==0);CHECK(rows[2].region==1);
    CHECK(rows[3].region==2);CHECK(rows[4].region==3);
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
    std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> transient_layout;
    const std::vector<Inputs::LLDrawInfo*> members{&transient_info};
    CHECK_FALSE(Inputs::ALSceneGroupPublicationNative::seal(transient_group,transient_dependencies,transient_layout,f.model,members,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount));
    CHECK_FALSE(transient_group->publication());CHECK(transient_group->dirty());
    CHECK_FALSE(transient_dependencies);
    // An actual raw reader remains valid outside cache residency. Retrying
    // with that reader cannot turn an unbudgeted group into a publisher.
    Inputs::ALScenePublicationNative::WorldFlavor flavor;flavor.indexed_textures=true;
    auto raw_reader=Inputs::ALScenePublicationNative::world(transient_info,flavor,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(raw_reader);
    CHECK_FALSE(transient_info.state->publication());
    auto rejected=Inputs::ALSceneGroupPublicationNative::seal(transient_group,transient_dependencies,transient_layout,f.model,members,tiny,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);CHECK_FALSE(rejected);
    CHECK_FALSE(transient_group->publication());CHECK_FALSE(transient_dependencies);
    CHECK(tiny.entryCount()==0);
    ALNativeSourceWorkingSet enough(4,1024*1024,2);
    auto retry=Inputs::ALSceneGroupPublicationNative::seal(transient_group,transient_dependencies,transient_layout,f.model,members,enough,
        Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(retry);
    CHECK(retry->sources[0].prototype!=raw_reader.prototype);
    CHECK(retry->sources[0].prototype->vertices==raw_reader.prototype->vertices);
    CHECK(retry->sources[0].owner->producer.lock()==transient_info.state);
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

TEST_CASE("Actual native group churn refreshes only dirty CPU rows after child residency ages out",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;for(U32 i=0;i<4;++i)f.draw();
    ALNativeSourceWorkingSet bounded(16,1024*1024,2);
    auto packet=f.seal(bounded,1);REQUIRE(packet);REQUIRE(f.group_layout);
    const auto old_rows=f.group_layout->sources;
    const auto old_union=f.group_layout->resources;
    const auto old_dependencies=f.group_dependencies;
    for(U32 frame=2;frame<=5;++frame)
    {REQUIRE(f.current(bounded,frame));bounded.trim(frame);}
    for(const auto& info:f.infos)CHECK_FALSE(info->state->publication());
    packet.reset();gGL.frameFences().release();
    const auto reads=f.reads();const auto before=Inputs::ALSceneGroupPublicationNative::statistics;
    std::array<std::array<U64,2>,4> before_rows{};
    for(U32 row=0;row<4;++row)
        before_rows[row]={f.infos[row]->publication_reads,f.infos[row]->dependency_walks};
    const auto uploads=LLVertexBuffer::metalUploadStats();
    const std::array<LLColor4U,3> green{LLColor4U(0,255,0,255),LLColor4U(0,255,0,255),LLColor4U(0,255,0,255)};
    f.infos[1]->mVertexBuffer->setColorData(green.data());f.infos[1]->mVertexBuffer->unmapBuffer();
    CHECK_FALSE(f.group->publication());CHECK(f.group_layout->sources==old_rows);
    CHECK(LLVertexBuffer::metalUploadStats().leased_orphans==uploads.leased_orphans);
    auto refreshed=f.seal(bounded,6);REQUIRE(refreshed);REQUIRE(refreshed->sources.size()==4);
    const auto after=Inputs::ALSceneGroupPublicationNative::statistics;
    CHECK(after.cpu_source_builds==before.cpu_source_builds+1);
    CHECK(after.cpu_source_reuses==before.cpu_source_reuses+3);
    CHECK(after.resource_union_builds==before.resource_union_builds);
    CHECK(f.group_layout->resources==old_union);CHECK(f.group_dependencies==old_dependencies);
    for(U32 row=0;row<4;++row)
    {
        if(row==1)CHECK(refreshed->sources[row].owner!=old_rows[row]);
        else
        {
            CHECK(refreshed->sources[row].owner==old_rows[row]);
            CHECK(f.infos[row]->publication_reads==before_rows[row][0]);
            CHECK(f.infos[row]->dependency_walks==before_rows[row][1]);
            CHECK_FALSE(f.infos[row]->state->publication());
        }
    }
    const auto updated_reads=f.reads();CHECK(updated_reads[0]>reads[0]);CHECK(updated_reads[1]>reads[1]);
    const auto row_reads=std::array<U64,3>{f.infos[0]->publication_reads,f.infos[2]->publication_reads,f.infos[3]->publication_reads};
    const auto group_revision=f.group->revision();
    // A second dirty row keeps every other CPU identity, and does not cause
    // any source getter/dependency walk for their aged-out child publishers.
    auto rows=f.group_layout->sources;refreshed.reset();gGL.frameFences().release();
    f.infos[1]->mVertexBuffer->setColorData(green.data());f.infos[1]->mVertexBuffer->unmapBuffer();
    auto again=f.seal(bounded,7);REQUIRE(again);CHECK(f.group->revision()!=group_revision);
    CHECK(f.infos[0]->publication_reads==row_reads[0]);CHECK(f.infos[2]->publication_reads==row_reads[1]);
    CHECK(f.infos[3]->publication_reads==row_reads[2]);
    CHECK(again->sources[0].owner==rows[0]);CHECK(again->sources[2].owner==rows[2]);CHECK(again->sources[3].owner==rows[3]);
}

TEST_CASE("Actual native CPU layout weak generations cannot replay expired resources",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    GroupPublication f;auto& info=f.draw();auto packet=f.seal();REQUIRE(packet);
    const auto old_record=f.group_layout->sources[0];
    const auto producer_revision=info.state->revision();
    std::weak_ptr<const void> generation=old_record->resources[0].allocation;
    packet.reset();f.group->evict();info.state->evict();gGL.frameFences().release();
    // Deliberately detach the old fixture signal and omit a binding event to
    // exercise the independent weak-generation proof. Actual owners retain
    // that subscription and invalidate on buffer destruction/rebinding.
    info.mVertexBuffer->nativePublication().forget(info.state);
    info.mVertexBuffer=f.buffer();
    REQUIRE(generation.expired());CHECK(info.state->revision()==producer_revision);
    const auto before=Inputs::ALSceneGroupPublicationNative::statistics.cpu_source_builds;
    auto fresh=f.seal();REQUIRE(fresh);CHECK(fresh->sources[0].owner!=old_record);
    CHECK(fresh->sources[0].prototype->vertices==info.mVertexBuffer->getMetalVertexBuffer());
    CHECK(Inputs::ALSceneGroupPublicationNative::statistics.cpu_source_builds==before+1);
    CHECK_FALSE(fresh->sources[0].owner->resources[0].allocation.expired());
}

TEST_CASE("Actual native CPU layout cache bounds rows bytes and publishers without dropping readers",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    using namespace Inputs::ALSceneGroupPublicationNative;
    struct PolicyRestore
    {
        CpuLayoutLimits saved=cpu_layouts.limits();
        PolicyRestore(){auto zero=saved;zero.maximum_layouts=0;cpu_layouts.configure(zero);}
        ~PolicyRestore(){auto zero=saved;zero.maximum_layouts=0;cpu_layouts.configure(zero);cpu_layouts.configure(saved);}
    } restore;
    CpuLayoutLimits limits;limits.maximum_layouts=1;limits.maximum_rows=4;limits.maximum_bytes=1024*1024;
    cpu_layouts.configure(limits);
    GroupPublication f;f.draw();auto old=f.seal();REQUIRE(old);REQUIRE(f.group_layout);
    auto old_layout=f.group_layout;const auto old_record=old->sources[0].owner;
    const auto initial=cpuLayoutStats();CHECK(initial.layouts==1);CHECK(initial.rows==1);CHECK(initial.estimated_bytes>sizeof(Source));
    Inputs::LLDrawInfo second;second.mVertexBuffer=f.buffer();second.mTexture.value=&f.image;second.mModelMatrix=&f.model;
    auto second_state=std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> second_dependencies;
    std::shared_ptr<Layout> second_layout;
    auto next=seal(second_state,second_dependencies,second_layout,f.model,std::vector<Inputs::LLDrawInfo*>{&second},
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);CHECK_FALSE(next);
    CHECK_FALSE(second_state->publication());CHECK_FALSE(second_layout);
    CHECK(f.current(Inputs::sNativeSceneSources,Inputs::gFrameCount)==old);
    CHECK(cpuLayoutStats().layouts==1);CHECK(cpuLayoutStats().rejections==initial.rejections+1);
    // The next frame may replace an older subset; the same frame must not
    // repeatedly discard and rebuild it for subsequent main/probe views.
    next=seal(second_state,second_dependencies,second_layout,f.model,std::vector<Inputs::LLDrawInfo*>{&second},
        Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount+1);REQUIRE(next);
    const auto evicted=cpuLayoutStats();CHECK(evicted.layouts==1);CHECK(evicted.rows==1);
    CHECK(evicted.evictions==initial.evictions+1);CHECK_FALSE(f.group->publication());
    CHECK(old_layout->sources.empty());CHECK(old_layout->sources.capacity()==0);CHECK_FALSE(old_layout->resources);
    CHECK(old->sources[0].owner==old_record);CHECK(old->sources[0].prototype->vertices);
    CHECK(f.copied<F32>(*old->sources[0].prototype->vertices,old->sources[0].prototype->position_offset,4)[0]==-1.f);
    // Native cache admission alone cannot keep an unbudgeted CPU publisher.
    for(U32 reason=0;reason<2;++reason)
    {
        auto rejected_limits=limits;
        if(reason==0)rejected_limits.maximum_rows=0;else rejected_limits.maximum_bytes=1;
        cpu_layouts.configure(rejected_limits);
        Inputs::LLDrawInfo rejected;rejected.mVertexBuffer=f.buffer();rejected.mTexture.value=&f.image;rejected.mModelMatrix=&f.model;
        auto rejected_state=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> rejected_dependencies;
        std::shared_ptr<Layout> rejected_layout;
        const auto before_rejection=cpuLayoutStats().rejections;
        CHECK_FALSE(seal(rejected_state,rejected_dependencies,rejected_layout,f.model,
            std::vector<Inputs::LLDrawInfo*>{&rejected},Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount));
        CHECK_FALSE(rejected_state->publication());CHECK_FALSE(rejected_layout);
        CHECK(cpuLayoutStats().rejections==before_rejection+1);
        CHECK(cpuLayoutStats().rows==0);CHECK(cpuLayoutStats().estimated_bytes==0);
    }
}

TEST_CASE("Actual native group CPU eviction callbacks preserve reentrant replacements and accounting",
          "[almetal][pipeline-buffer-publication][native-group-publication]")
{
    using namespace Inputs::ALSceneGroupPublicationNative;
    struct PolicyRestore
    {
        CpuLayoutLimits saved=cpu_layouts.limits();
        PolicyRestore(){auto zero=saved;zero.maximum_layouts=0;cpu_layouts.configure(zero);}
        ~PolicyRestore(){auto zero=saved;zero.maximum_layouts=0;cpu_layouts.configure(zero);cpu_layouts.configure(saved);}
    } restore;
    struct Reenter final : ALNativePublication::Observer
    {
        std::function<void()> action;bool fired=false,failed=false;
        void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
        {
            if(phase!=ALNativePublication::Phase::withdrawn || fired)return;
            fired=true;try{action();}catch(...){failed=true;}
        }
    };
    GroupPublication f;auto& info=f.draw();const std::vector<Inputs::LLDrawInfo*> members{&info};
    for(bool replace:{false,true})
    {
        CAPTURE(replace);
        const U32 frame=replace?5u:1u;
        auto zero=restore.saved;zero.maximum_layouts=0;cpu_layouts.configure(zero);
        CpuLayoutLimits limits;limits.maximum_layouts=2;limits.maximum_rows=8;limits.maximum_bytes=1024*1024;
        cpu_layouts.configure(limits);
        auto a=std::make_shared<ALNativePublication::State>(),b=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> a_dependencies,b_dependencies;
        std::shared_ptr<Layout> a_layout,b_layout;
        auto pa=seal(a,a_dependencies,a_layout,f.model,members,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,frame);
        auto pb=seal(b,b_dependencies,b_layout,f.model,members,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,frame);
        REQUIRE(pa);REQUIRE(pb);CHECK(cpuLayoutStats().layouts==2);
        auto target=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> target_dependencies;
        std::shared_ptr<Layout> target_layout;
        std::shared_ptr<const Payload> queued_t1,replacement;
        std::shared_ptr<Layout> installed_layout;
        std::shared_ptr<ALNativePublication::Observer> installed_dependencies;
        auto observer=std::make_shared<Reenter>();
        observer->action=[&]
        {
            queued_t1=std::static_pointer_cast<const Payload>(target->publication());
            if(replace)
            {
                replacement=seal(target,target_dependencies,target_layout,f.model,members,
                    Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,frame+1);
                installed_layout=target_layout;installed_dependencies=target_dependencies;
            }
            else target->invalidate();
        };
        a->observe(observer);
        const auto rejected_before=cpuLayoutStats().rejections;
        auto stale=seal(target,target_dependencies,target_layout,f.model,members,
            Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,frame+1);
        CHECK_FALSE(stale);CHECK(observer->fired);CHECK_FALSE(observer->failed);REQUIRE(queued_t1);
        CHECK(cpuLayoutStats().rejections==rejected_before+1);
        CHECK(b->publication().get()==pb.get()); // The stale outer attempt cannot evict the second victim.
        if(replace)
        {
            REQUIRE(replacement);CHECK(target->publication().get()==replacement.get());
            CHECK(target_layout==installed_layout);CHECK(target_dependencies==installed_dependencies);
            CHECK(replacement->serial!=queued_t1->serial);
            CHECK(current(target,f.model,Inputs::sNativeSceneSources,frame+1)==replacement);
            CHECK(cpuLayoutStats().layouts==2);CHECK(cpuLayoutStats().rows==2);
            // Subsequent pressure must find the newer identity/serial, with
            // no stale duplicate node to erase its lookup or double charge.
            auto c=std::make_shared<ALNativePublication::State>();
            std::shared_ptr<ALNativePublication::Observer> c_dependencies;std::shared_ptr<Layout> c_layout;
            auto pc=seal(c,c_dependencies,c_layout,f.model,members,
                Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,frame+2);REQUIRE(pc);
            CHECK(current(target,f.model,Inputs::sNativeSceneSources,frame+2)==replacement);
            CHECK(cpuLayoutStats().layouts==2);CHECK(cpuLayoutStats().rows==2);
        }
        else
        {
            CHECK_FALSE(target->publication());CHECK_FALSE(target_layout);CHECK_FALSE(target_dependencies);
            CHECK(cpuLayoutStats().layouts==1);CHECK(cpuLayoutStats().rows==1);
        }
        CHECK(queued_t1->sources.size()==1);CHECK(queued_t1->resources.size()>0);
        CHECK(f.copied<F32>(*queued_t1->sources[0].prototype->vertices,
            queued_t1->sources[0].prototype->position_offset,4)[0]==-1.f);
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
    U64 native_epoch=0;
    struct Group
    {
        std::shared_ptr<ALNativePublication::State> state=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> dependencies;
        std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
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
    {
        std::sort(values.begin(),values.end());const auto middle=values.size()/2;
        return values.size()%2?values[middle]:(values[middle-1]+values[middle])*.5;
    };
    constexpr U32 repetitions=12,warmups=2,draws_per_group=16;
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
            {group.state->invalidate();group.layout.reset();for(auto* info:group.members)info->state->invalidate();}
            struct Costs
            {
                std::array<double,repetitions> cpu{},encode{},complete{},gpu{};
                U64 group_draws=0,source_publications=0,source_only_publications=0,source_reads=0,dependency_walks=0,image_reads=0;
                U64 group_seals=0,group_declines=0,published_bytes=0,gpu_ready=0;
            };
            std::array<Costs,2> costs;
            for(U32 sample=0;sample<warmups+repetitions;++sample)
            {
                std::array<std::array<GroupPublication::Image,3>,2> verified;
                // Both arms model this same viewer frame. Alternating their
                // execution order must not age one arm's source cache by an
                // artificial three frames between consecutive samples.
                ++Inputs::gFrameCount;
                // Alternate the pair's order to reduce a fixed warm-cache or
                // thermal ordering advantage without mixing their producers.
                for(U32 position=0;position<2;++position)
                {
                    const U32 mode=(sample+position)%2;auto& arm=arms[mode];
                    const auto before_reads=f.reads();const auto source_serial=Inputs::sNativeSceneSerial;
                    const auto source_rows_before=Inputs::ALSceneGroupPublicationNative::statistics.cpu_source_builds;
                    const auto bytes_before=frame_data->stats().published_bytes;
                    U64 admitted=0,seals=0,declines=0;
                    std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                    std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                    const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                    // Native constants belong to this arm only; main and its
                    // two probes share an epoch, but paired arms never do.
                    frame_data->beginFrame(++native_epoch);
                    if(condition==0)
                    {
                        // Cold source/packet caches, with the producer event
                        // and every publication/admission inside the interval.
                        for(auto& group:arm.groups)
                        {group.state->invalidate();group.layout.reset();for(auto* info:group.members)info->state->invalidate();}
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
                        capture.complete=true;capture.clearDraws();capture.world_visibility_members.clear();
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
                                if(!Inputs::ALSceneGroupPublicationNative::seal(group.state,group.dependencies,group.layout,f.model,group.members,
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
                        const U64 serials=Inputs::sNativeSceneSerial-source_serial;
                        const U64 expected_sources=condition==0?count:condition==1?0:arm.groups.size();
                        // Each successful native group seal consumes one
                        // serial in addition to actual source publications.
                        REQUIRE(serials>=seals);CHECK(serials-seals==expected_sources);
                        if(mode)CHECK(Inputs::ALSceneGroupPublicationNative::statistics.cpu_source_builds-source_rows_before==expected_sources);
                        cost.source_publications+=serials;cost.source_only_publications+=serials-seals;
                        cost.source_reads+=after_reads[0]-before_reads[0];cost.dependency_walks+=after_reads[1]-before_reads[1];
                        cost.image_reads+=after_reads[2]-before_reads[2];
                        cost.published_bytes+=frame_data->stats().published_bytes-bytes_before;
                        if(mode && condition==1)CHECK(admitted==U64(count)*views);
                        CHECK(declines==0);
                    }
                    arm.cull.clear();arm.pipeline.mMetalWorldSimpleCapture->clearDraws();
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
                    <<" owner_cpu_max_ms="<<*std::max_element(cost.cpu.begin(),cost.cpu.end())
                    <<" capture_encode_max_ms="<<*std::max_element(cost.encode.begin(),cost.encode.end())
                    <<" completion_envelope_median_ms="<<median(cost.complete)<<" gpu_sum_median_ms="<<median(cost.gpu)
                    <<" completion_envelope_max_ms="<<*std::max_element(cost.complete.begin(),cost.complete.end())
                    <<" gpu_sum_max_ms="<<*std::max_element(cost.gpu.begin(),cost.gpu.end())
                    <<" gpu_ready_commands="<<cost.gpu_ready<<" admitted_native_draws="<<cost.group_draws
                    <<" group_seal_attempts="<<cost.group_seals<<" group_declines="<<cost.group_declines
                    <<" source_and_group_serials="<<cost.source_publications<<" source_only_publications="<<cost.source_only_publications
                    <<" source_getter_reads="<<cost.source_reads
                    <<" dependency_walks="<<cost.dependency_walks<<" image_getter_reads="<<cost.image_reads
                    <<" native_frame_bytes="<<cost.published_bytes
                    <<" paired_logical_frame=1 separate_arm_native_epochs=1"
                    <<" geometry_creation_excluded=1 identity_camera=1 whole_viewer_fps_claim=0\n";
                for(U32 sample=0;sample<repetitions;++sample)
                    std::cout<<"NATIVE_GROUP_PATH_RAW draws_per_view="<<count<<" views="<<views
                        <<" condition="<<condition<<" mode="<<mode<<" sample="<<sample
                        <<" owner_cpu_ms="<<cost.cpu[sample]<<" capture_encode_ms="<<cost.encode[sample]
                        <<" completion_envelope_ms="<<cost.complete[sample]<<" gpu_sum_ms="<<cost.gpu[sample]<<'\n';
            }
        }
    }
}

TEST_CASE("Actual published compact capture complete path costs",
          "[almetal][.native-compact-capture-cost]")
{
    GroupPublication f;
    auto main=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);
    auto probes=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);
    auto data=ALMetalWorldFrameData::create(*f.device,&f.error);
    INFO(f.error);REQUIRE(main);REQUIRE(probes);REQUIRE(data);main->setFrameData(data);probes->setFrameData(data);
    U64 native_epoch=0;
    struct Arm
    {
        Inputs::LLPipeline pipeline;
        std::vector<Inputs::LLDrawInfo*> sources;
        std::vector<ALMetalWorldVisibilityQuery> queries;
    };
    const auto cpu_ns=[]
    {
        timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
        return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;
    };
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    constexpr U32 samples=12,warmups=2;
    for(U32 count:{1u,4u,16u,512u})
    {
        std::array<Arm,2> arms;
        for(auto& arm:arms)for(U32 source=0;source<count;++source)arm.sources.push_back(&f.draw());
        for(U32 views:{1u,3u})for(U32 condition=0;condition<3;++condition)
        {
            for(auto& arm:arms)for(auto* info:arm.sources)info->state->invalidate();
            struct Costs
            {
                std::array<double,samples> cpu{},encode{},complete{},gpu{};
                U64 source_publications=0,native_bytes=0,raw_bytes=0,reference_bytes=0,order_bytes=0;
                U64 source_getters=0,dependencies=0,image_getters=0,gpu_ready=0;
                U64 cpu_description_owners=0,native_tuple_owners=0;
            };
            std::array<Costs,2> costs;
            for(U32 sample=0;sample<samples+warmups;++sample)
            {
                std::array<std::array<GroupPublication::Image,3>,2> pixels;
                ++Inputs::gFrameCount; // One logical viewer frame for the paired arms.
                for(U32 position=0;position<2;++position)
                {
                    const U32 mode=(sample+position)%2;auto& arm=arms[mode];
                    auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                    const auto before_reads=f.reads();const auto before_serial=Inputs::sNativeSceneSerial;
                    const auto before_bytes=data->stats().published_bytes;
                    U64 raw_bytes=0,reference_bytes=0,order_bytes=0,cpu_owners=0,native_owners=0;
                    std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                    std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                    const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                    data->beginFrame(++native_epoch); // Independent arm; shared by main plus both probes.
                    if(condition==0)for(auto* info:arm.sources)info->state->invalidate();
                    if(condition==2)
                    {
                        const LLColor4U tint=sample%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                        const std::array<LLColor4U,3> changed{tint,tint,tint};
                        for(U32 source=0;source<count;source+=16)
                        {auto& vb=*arm.sources[source]->mVertexBuffer;vb.setColorData(changed.data());vb.unmapBuffer();}
                    }
                    for(U32 view=0;view<views;++view)
                    {
                        capture.clearDraws();capture.complete=true;capture.compact_gbuffer_active=mode!=0;
                        capture.world_resources=std::make_shared<Inputs::WorldResources>();
                        for(auto* info:arm.sources)arm.pipeline.captureMetalWorldGBufferBatch(*info,true);
                        REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==count);REQUIRE(capture.flushDrawOrder());
                        raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                        reference_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);
                        order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);
                        cpu_owners+=capture.cpu_sources?capture.cpu_sources->owners.size():0;
                        native_owners+=capture.world_resources->owners.size();
                        auto scene=f.scene(capture);
                        auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),
                            std::move(capture.source_references),std::move(capture.draw_order),std::move(arm.queries),
                            std::move(capture.cpu_sources),capture.world_resources,Inputs::gFrameCount,&f.error);
                        INFO(f.error);REQUIRE(inputs);scene={};
                        auto& renderer=view==0?*main:*probes;
                        REQUIRE(renderer.beginCommandBatch("Actual published compact capture cost",&f.error));
                        targets[view]=renderer.renderOwnedInputs(*inputs,&f.error);INFO(f.error);REQUIRE(targets[view]);
                        completions[view]=renderer.commandBatchCompletion();
                        // CPU retirement belongs to the measured interval and
                        // does not await GPU completion or retain descriptions
                        // through the native-only resource aggregate.
                        REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,
                            capture.source_references,capture.draw_order,arm.queries));
                        capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
                        REQUIRE(renderer.commitCommandBatch(&f.error));
                    }
                    const auto owner_cpu=cpu_ns()-begin_cpu;const auto encoded=std::chrono::steady_clock::now();
                    REQUIRE(f.device->waitUntilIdle(&f.error));const auto completed=std::chrono::steady_clock::now();
                    gGL.frameFences().release();
                    if(sample==warmups-1)for(U32 view=0;view<views;++view)pixels[mode][view]=f.read(*targets[view]);
                    if(sample>=warmups)
                    {
                        const auto at=sample-warmups;auto& cost=costs[mode];const auto after_reads=f.reads();
                        cost.cpu[at]=owner_cpu/1e6;
                        cost.encode[at]=std::chrono::duration<double,std::milli>(encoded-begin).count();
                        cost.complete[at]=std::chrono::duration<double,std::milli>(completed-begin).count();
                        for(U32 view=0;view<views;++view)
                        {
                            ALMetalCommandGPUTime gpu;
                            if(completions[view] && completions[view]->gpuTime(gpu)==ALMetalCommandTimingStatus::ready)
                            {cost.gpu[at]+=gpu.elapsed_nanoseconds/1e6;++cost.gpu_ready;}
                        }
                        cost.source_publications+=Inputs::sNativeSceneSerial-before_serial;
                        cost.native_bytes+=data->stats().published_bytes-before_bytes;
                        cost.raw_bytes+=raw_bytes;cost.reference_bytes+=reference_bytes;cost.order_bytes+=order_bytes;
                        cost.cpu_description_owners+=cpu_owners;cost.native_tuple_owners+=native_owners;
                        cost.source_getters+=after_reads[0]-before_reads[0];cost.dependencies+=after_reads[1]-before_reads[1];
                        cost.image_getters+=after_reads[2]-before_reads[2];
                        const U64 expected_sources=condition==0?count:condition==1?0:(count+15)/16;
                        CHECK(Inputs::sNativeSceneSerial-before_serial==expected_sources);
                        if(mode)CHECK(raw_bytes==0);else CHECK(reference_bytes==0);
                    }
                }
                if(sample==warmups-1)for(U32 view=0;view<views;++view)CHECK(pixels[0][view]==pixels[1][view]);
            }
            for(U32 mode=0;mode<2;++mode)
            {
                const auto& cost=costs[mode];
                std::cout<<"NATIVE_COMPACT_CAPTURE_COST draws_per_view="<<count<<" views="<<views
                    <<" condition="<<(condition==0?"cold_source_publication":condition==1?"warm":"churn")
                    <<" update_stride=16 updated_sources_per_frame="<<(condition==2?(count+15)/16:0)
                    <<" mode="<<(mode?"source_reference":"raw_batch")<<" samples="<<samples<<" warmups="<<warmups
                    <<" owner_cpu_median_ms="<<median(cost.cpu)<<" owner_cpu_max_ms="<<*std::max_element(cost.cpu.begin(),cost.cpu.end())
                    <<" capture_adopt_encode_retire_median_ms="<<median(cost.encode)
                    <<" capture_adopt_encode_retire_max_ms="<<*std::max_element(cost.encode.begin(),cost.encode.end())
                    <<" completion_envelope_median_ms="<<median(cost.complete)<<" completion_envelope_max_ms="<<*std::max_element(cost.complete.begin(),cost.complete.end())
                    <<" gpu_sum_median_ms="<<median(cost.gpu)<<" gpu_sum_max_ms="<<*std::max_element(cost.gpu.begin(),cost.gpu.end())
                    <<" gpu_ready_commands="<<cost.gpu_ready<<" source_publications="<<cost.source_publications
                    <<" raw_value_bytes="<<cost.raw_bytes<<" reference_value_bytes="<<cost.reference_bytes<<" order_value_bytes="<<cost.order_bytes
                    <<" cpu_description_owner_pins="<<cost.cpu_description_owners<<" native_tuple_owner_pins="<<cost.native_tuple_owners
                    <<" native_frame_bytes="<<cost.native_bytes<<" source_getters="<<cost.source_getters
                    <<" dependency_walks="<<cost.dependencies<<" image_getters="<<cost.image_getters
                    <<" group_path=0 unique_drawinfo_sources="<<count
                    <<" paired_logical_frame=1 separate_arm_native_epochs=1"
                    <<" geometry_texture_drawinfo_creation_excluded=1 viewer_dependency_frontier=typed_native_signals whole_viewer_fps_claim=0\n";
                for(U32 sample=0;sample<samples;++sample)
                    std::cout<<"NATIVE_COMPACT_CAPTURE_RAW draws_per_view="<<count<<" views="<<views
                        <<" condition="<<condition<<" mode="<<mode<<" sample="<<sample
                        <<" owner_cpu_ms="<<cost.cpu[sample]<<" capture_adopt_encode_retire_ms="<<cost.encode[sample]
                        <<" completion_envelope_ms="<<cost.complete[sample]<<" gpu_sum_ms="<<cost.gpu[sample]<<'\n';
            }
        }
    }
}

TEST_CASE("Actual Legacy and PBR source rows include capture publication encoding and retirement costs",
          "[almetal][.native-source-rows-cost]")
{
    // Geometry/material construction is an explicit fixture frontier. Every
    // sample charges the actual factory, model registration, metadata, native
    // row/page acquisition, command encoding, CPU retirement and submission.
    // This is a complete contained path comparison, never viewer FPS evidence.
    constexpr U32 samples=12,warmups=2;
    const auto cpu_ns=[]
    {
        timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
        return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;
    };
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    for(U32 family:{1u,2u})
    {
        std::vector<std::unique_ptr<Inputs::LLFetchedGLTFMaterial>> material_owners;
        GroupPublication f;ALNativeRegionModel region;
        REQUIRE(region.update(f.model,LLVector3(0.f,0.f,0.f)));
        Inputs::gSavedSettings.native_resources=false;Inputs::gSavedSettings.native_visibility=false;
        auto main=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);
        auto probes=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);
        INFO(f.error);REQUIRE(main);REQUIRE(probes);REQUIRE(data);
        main->setFrameData(data);probes->setFrameData(data);U64 native_epoch=0;
        struct Arm
        {
            Inputs::LLPipeline pipeline;
            std::vector<Inputs::LLDrawInfo*> sources;
            std::vector<ALMetalWorldVisibilityQuery> queries;
        };
        struct ContextCosts
        {
            U64 requested=0,bound=0,fallback=0,setup_failures=0;
            U64 page_requests=0,hits=0,publications=0,failures=0,encoded_bytes=0;
            U64 copied_page_bytes=0,copied_bank_bytes=0,view_bytes=0,page_allocations=0,bank_allocations=0;
            std::size_t maximum_rows=0,maximum_live_metadata_bytes=0;
        };
        for(U32 count:{1u,4u,16u,512u})
        {
            std::array<Arm,2> arms;
            for(auto& arm:arms)for(U32 source=0;source<count;++source)
            {
                auto& info=f.draw();
                if(family==2)
                {
                    info.mVertexBuffer=f.buffer(true,LLVertexBuffer::MAP_TANGENT);
                    std::array<LLVector4a,3> tangents;for(auto& tangent:tangents)tangent.set(1.f,0.f,0.f,1.f);
                    info.mVertexBuffer->setTangentData(tangents.data());info.mVertexBuffer->unmapBuffer();
                    auto material=std::make_unique<Inputs::LLFetchedGLTFMaterial>();
                    info.mGLTFMaterial.value=material.get();material_owners.push_back(std::move(material));
                }
                arm.sources.push_back(&info);
            }
            for(U32 views:{1u,3u})for(U32 condition=0;condition<3;++condition)
            {
                CAPTURE(family,count,views,condition);
                for(auto& arm:arms)for(auto* info:arm.sources)info->state->invalidate();
                struct Costs
                {
                    std::array<double,samples> cpu{},encode{},complete{},gpu{};
                    std::array<U64,samples> publication_samples{},requested_samples{},bound_samples{},fallback_samples{};
                    std::array<std::array<U64,samples>,2> page_publication_samples{};
                    U64 publications=0,native_frame_bytes=0,raw_bytes=0,reference_bytes=0,order_bytes=0;
                    U64 candidate_bytes=0,region_bytes=0,cpu_pins=0,native_pins=0,getters=0,dependencies=0,image_getters=0,gpu_ready=0;
                    U64 enabled_views=0,registered_models=0,ordinary_requests=0,candidates=0,unsupported=0,owner_skips=0,metadata_failures=0;
                    std::array<ContextCosts,2> contexts;
                };
                std::array<Costs,2> costs;
                for(U32 sample=0;sample<samples+warmups;++sample)
                {
                    std::array<std::array<GroupPublication::Image,3>,2> pixels;
                    std::array<std::array<std::array<std::uint64_t,2>,3>,2> query_counts{};
                    ++Inputs::gFrameCount; // Paired arms see ONE logical viewer/source-cache frame.
                    for(U32 position=0;position<2;++position)
                    {
                        const U32 mode=(sample+position)%2;auto& arm=arms[mode];
                        CAPTURE(mode,sample);
                        auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                        const auto before_reads=f.reads();const auto before_serial=Inputs::sNativeSceneSerial;
                        const auto before_bytes=data->stats().published_bytes;
                        const auto before_capture=capture.source_rows_stats;
                        const std::array<ALMetalWorldSourceDrawStats,2> before_context{main->worldSourceStats(),probes->worldSourceStats()};
                        U64 raw_bytes=0,reference_bytes=0,order_bytes=0,candidate_bytes=0,region_bytes=0,cpu_pins=0,native_pins=0;
                        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                        std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                        const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                        data->beginFrame(++native_epoch); // Native constants never deduplicate between paired arms.
                        if(condition==0)for(auto* info:arm.sources)info->state->invalidate();
                        if(condition==2)
                        {
                            const LLColor4U tint=sample%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                            const std::array<LLColor4U,3> changed{tint,tint,tint};
                            for(U32 source=0;source<count;source+=16)
                            {auto& vb=*arm.sources[source]->mVertexBuffer;vb.setColorData(changed.data());vb.unmapBuffer();}
                        }
                        for(U32 view=0;view<views;++view)
                        {
                            capture.clearDraws();capture.complete=true;
                            capture.beginSourceRows(mode!=0);capture.compact_gbuffer_active=mode!=0;
                            if(mode)capture.registerSourceModel(&f.model,region.snapshot());
                            capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            for(auto* info:arm.sources)arm.pipeline.captureMetalPublishedWorldBatch(*info,family,false);
                            REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==count);REQUIRE(capture.flushDrawOrder());
                            raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                            reference_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);
                            order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);
                            cpu_pins+=capture.cpu_sources?capture.cpu_sources->owners.size():0;
                            native_pins+=capture.world_resources->owners.size();
                            if(mode)
                            {
                                REQUIRE(capture.source_rows);REQUIRE(capture.source_rows->candidates.size()==count);
                                REQUIRE(capture.source_rows->regions.size()==1);
                                candidate_bytes+=count*sizeof(ALMetalWorldSourceView::Candidate);
                                region_bytes+=sizeof(ALMetalWorldSourceRegion);
                            }
                            else REQUIRE_FALSE(capture.source_rows);
                            arm.queries.resize(2);
                            for(U32 query=0;query<2;++query)
                            {
                                auto& value=arm.queries[query];value={};
                                std::memcpy(value.modelview_projection,f.model.getF32ptr(),sizeof(value.modelview_projection));
                                value.center[0]=.35f;value.center[1]=-.5f;value.center[2]=.75f;
                                value.half_extent[0]=value.half_extent[1]=value.half_extent[2]=.08f;
                                value.batch_offset=query?count:count/2;value.fan=7;value.count_samples=1;
                            }
                            auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                            // Baseline is the currently enabled ordinary native
                            // authored-batch consumer, not argument tables or
                            // compact references detached from source capture.
                            scene.material_argument_buffers=false;
                            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),
                                std::move(capture.source_references),std::move(capture.draw_order),std::move(arm.queries),
                                std::move(capture.cpu_sources),capture.world_resources,native_epoch,&f.error);
                            INFO(f.error);REQUIRE(inputs);scene={};
                            auto& renderer=view==0?*main:*probes;
                            REQUIRE(renderer.beginCommandBatch("Actual Legacy/PBR source-row complete cost",&f.error));
                            targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                            completions[view]=renderer.commandBatchCompletion();
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,
                                capture.source_references,capture.draw_order,arm.queries));
                            capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            REQUIRE(renderer.commitCommandBatch(&f.error));
                        }
                        const auto owner_cpu=cpu_ns()-begin_cpu;const auto encoded=std::chrono::steady_clock::now();
                        REQUIRE(f.device->waitUntilIdle(&f.error));const auto completed=std::chrono::steady_clock::now();
                        gGL.frameFences().release();
                        const auto after_reads=f.reads();const auto after_capture=capture.source_rows_stats;
                        const std::array<ALMetalWorldSourceDrawStats,2> after_context{main->worldSourceStats(),probes->worldSourceStats()};
                        const U64 publications=Inputs::sNativeSceneSerial-before_serial;
                        const U64 expected_sources=condition==0?count:condition==1?0:(count+15)/16;
                        U64 requested=0,bound=0,fallback=0,setup_failures=0;
                        for(U32 owner=0;owner<2;++owner)
                        {
                            requested+=after_context[owner].requested-before_context[owner].requested;
                            bound+=after_context[owner].bound-before_context[owner].bound;
                            fallback+=after_context[owner].fallback-before_context[owner].fallback;
                            setup_failures+=after_context[owner].setup_failures-before_context[owner].setup_failures;
                        }
                        CHECK(requested==(mode?U64(count)*views:0));CHECK(bound==(mode?U64(count)*views:0));
                        CHECK(fallback==0);CHECK(setup_failures==0);
                        if(sample==warmups-1)for(U32 view=0;view<views;++view)
                        {
                            pixels[mode][view]=f.read(*targets[view]);
                            for(U32 query=0;query<2;++query)REQUIRE(results[view]->wait(query,query_counts[mode][view][query],&f.error));
                        }
                        if(sample<warmups)
                        {
                            // Preserve actual first-use/native setup observations
                            // instead of hiding them inside discarded warmups.
                            std::cout<<"NATIVE_SOURCE_ROWS_WARMUP family="<<(family==1?"legacy":"pbr")
                                <<" draws_per_view="<<count<<" views="<<views<<" condition="<<condition<<" mode="<<mode<<" sample="<<sample
                                <<" owner_cpu_ms="<<owner_cpu/1e6
                                <<" capture_adopt_encode_retire_ms="<<std::chrono::duration<double,std::milli>(encoded-begin).count()
                                <<" completion_envelope_ms="<<std::chrono::duration<double,std::milli>(completed-begin).count()
                                <<" source_publications="<<publications<<" requested="<<requested<<" bound="<<bound
                                <<" fallback="<<fallback<<" setup_failures="<<setup_failures<<'\n';
                        }
                        else
                        {
                            CHECK(publications==expected_sources);const auto at=sample-warmups;auto& cost=costs[mode];
                            cost.publication_samples[at]=publications;cost.requested_samples[at]=requested;
                            cost.bound_samples[at]=bound;cost.fallback_samples[at]=fallback;
                            cost.cpu[at]=owner_cpu/1e6;cost.encode[at]=std::chrono::duration<double,std::milli>(encoded-begin).count();
                            cost.complete[at]=std::chrono::duration<double,std::milli>(completed-begin).count();
                            for(U32 view=0;view<views;++view)
                            {
                                ALMetalCommandGPUTime time;
                                if(completions[view]&&completions[view]->gpuTime(time)==ALMetalCommandTimingStatus::ready)
                                {cost.gpu[at]+=time.elapsed_nanoseconds/1e6;++cost.gpu_ready;}
                            }
                            cost.publications+=publications;cost.native_frame_bytes+=data->stats().published_bytes-before_bytes;
                            cost.raw_bytes+=raw_bytes;cost.reference_bytes+=reference_bytes;cost.order_bytes+=order_bytes;
                            cost.candidate_bytes+=candidate_bytes;cost.region_bytes+=region_bytes;cost.cpu_pins+=cpu_pins;cost.native_pins+=native_pins;
                            cost.getters+=after_reads[0]-before_reads[0];cost.dependencies+=after_reads[1]-before_reads[1];cost.image_getters+=after_reads[2]-before_reads[2];
                            cost.enabled_views+=after_capture.enabled_views-before_capture.enabled_views;
                            cost.registered_models+=after_capture.registered_models-before_capture.registered_models;
                            cost.ordinary_requests+=after_capture.requested_ordinary_rows-before_capture.requested_ordinary_rows;
                            cost.candidates+=after_capture.emitted_candidates-before_capture.emitted_candidates;
                            cost.unsupported+=after_capture.unsupported_skips-before_capture.unsupported_skips;
                            cost.owner_skips+=after_capture.owner_proof_skips-before_capture.owner_proof_skips;
                            cost.metadata_failures+=after_capture.optional_metadata_failures-before_capture.optional_metadata_failures;
                            for(U32 owner=0;owner<2;++owner)
                            {
                                const auto& previous=before_context[owner];const auto& current=after_context[owner];auto& context=cost.contexts[owner];
                                context.requested+=current.requested-previous.requested;context.bound+=current.bound-previous.bound;
                                context.fallback+=current.fallback-previous.fallback;context.setup_failures+=current.setup_failures-previous.setup_failures;
                                const auto& a=previous.pages;const auto& b=current.pages;
                                context.page_requests+=b.requests-a.requests;context.hits+=b.hits-a.hits;context.publications+=b.row_publications-a.row_publications;
                                cost.page_publication_samples[owner][at]=b.row_publications-a.row_publications;
                                context.failures+=b.failures-a.failures;context.encoded_bytes+=b.encoded_row_bytes-a.encoded_row_bytes;
                                context.copied_page_bytes+=b.copied_page_bytes-a.copied_page_bytes;context.copied_bank_bytes+=b.copied_bank_bytes-a.copied_bank_bytes;
                                context.view_bytes+=b.view_bytes-a.view_bytes;context.page_allocations+=b.page_allocations-a.page_allocations;
                                context.bank_allocations+=b.bank_allocations-a.bank_allocations;
                                context.maximum_rows=std::max(context.maximum_rows,b.rows);
                                context.maximum_live_metadata_bytes=std::max(context.maximum_live_metadata_bytes,b.live_metadata_bytes);
                            }
                            if(mode)
                            {
                                CHECK(after_capture.enabled_views-before_capture.enabled_views==views);
                                CHECK(after_capture.registered_models-before_capture.registered_models==views);
                                CHECK(after_capture.emitted_candidates-before_capture.emitted_candidates==U64(count)*views);
                                CHECK(raw_bytes==0);
                            }
                            else CHECK(reference_bytes==0);
                        }
                    }
                    if(sample==warmups-1)for(U32 view=0;view<views;++view)
                    {CHECK(pixels[0][view]==pixels[1][view]);CHECK(query_counts[0][view]==query_counts[1][view]);}
                }
                for(U32 mode=0;mode<2;++mode)
                {
                    const auto& cost=costs[mode];
                    std::cout<<"NATIVE_SOURCE_ROWS_COST family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                        <<" condition="<<(condition==0?"cold_source_publication":condition==1?"warm":"churn")
                        <<" update_stride=16 updated_sources_per_frame="<<(condition==2?(count+15)/16:0)
                        <<" mode="<<(mode?"native_source_rows":"ordinary_native")<<" samples="<<samples<<" warmups="<<warmups
                        <<" owner_cpu_median_ms="<<median(cost.cpu)<<" owner_cpu_max_ms="<<*std::max_element(cost.cpu.begin(),cost.cpu.end())
                        <<" capture_adopt_encode_retire_median_ms="<<median(cost.encode)<<" capture_adopt_encode_retire_max_ms="<<*std::max_element(cost.encode.begin(),cost.encode.end())
                        <<" completion_envelope_median_ms="<<median(cost.complete)<<" completion_envelope_max_ms="<<*std::max_element(cost.complete.begin(),cost.complete.end())
                        <<" gpu_sum_median_ms="<<median(cost.gpu)<<" gpu_sum_max_ms="<<*std::max_element(cost.gpu.begin(),cost.gpu.end())
                        <<" gpu_ready_commands="<<cost.gpu_ready<<" source_publications="<<cost.publications
                        <<" raw_value_bytes="<<cost.raw_bytes<<" reference_value_bytes="<<cost.reference_bytes<<" order_value_bytes="<<cost.order_bytes
                        <<" candidate_value_bytes="<<cost.candidate_bytes<<" regional_value_bytes="<<cost.region_bytes
                        <<" cpu_description_owner_pins="<<cost.cpu_pins<<" native_tuple_owner_pins="<<cost.native_pins
                        <<" native_frame_bytes="<<cost.native_frame_bytes<<" source_getters="<<cost.getters
                        <<" dependency_walks="<<cost.dependencies<<" image_getters="<<cost.image_getters
                        <<" enabled_views="<<cost.enabled_views<<" registered_models="<<cost.registered_models
                        <<" ordinary_requests="<<cost.ordinary_requests<<" emitted_candidates="<<cost.candidates
                        <<" unsupported_skips="<<cost.unsupported<<" owner_proof_skips="<<cost.owner_skips<<" metadata_failures="<<cost.metadata_failures
                        <<" legacy_material_slots="<<(family==1?1:0)<<" pbr_material_slots="<<(family==2?1:0)
                        <<" legacy_shader_mask="<<(family==1?0:-1)<<" material_values_shared=1 queries_per_view=2"
                        <<" unique_drawinfo_sources="<<count<<" separate_llvertexbuffer_objects="<<count
                        <<" paired_logical_frame=1 separate_arm_native_epochs=1 regional_models=1 identity_camera=1"
                        <<" baseline_argument_tables=0 native_contexts_retained=1"
                        <<" geometry_texture_drawinfo_creation_excluded=1 viewer_dependency_frontier=typed_native_signals whole_viewer_fps_claim=0\n";
                    for(U32 owner=0;owner<2;++owner)
                    {
                        const auto& context=cost.contexts[owner];
                        // Page contexts may be shared by main/probe. Report
                        // each renderer observation and never sum their pages.
                        std::cout<<"NATIVE_SOURCE_ROWS_CONTEXT_COST family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count
                            <<" views="<<views<<" condition="<<condition<<" mode="<<mode<<" renderer="<<(owner?"probe":"main")
                            <<" requested="<<context.requested<<" bound="<<context.bound<<" fallback="<<context.fallback<<" setup_failures="<<context.setup_failures
                            <<" page_requests="<<context.page_requests<<" page_hits="<<context.hits<<" row_publications="<<context.publications
                            <<" page_failures="<<context.failures<<" encoded_row_bytes="<<context.encoded_bytes
                            <<" copied_page_bytes="<<context.copied_page_bytes<<" copied_bank_bytes="<<context.copied_bank_bytes
                            <<" view_bytes="<<context.view_bytes<<" page_allocations="<<context.page_allocations<<" bank_allocations="<<context.bank_allocations
                            <<" maximum_rows="<<context.maximum_rows<<" maximum_live_metadata_bytes="<<context.maximum_live_metadata_bytes
                            <<" page_context_may_share=1 page_counters_not_summed=1\n";
                    }
                    for(U32 sample=0;sample<samples;++sample)
                        std::cout<<"NATIVE_SOURCE_ROWS_RAW family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count
                            <<" views="<<views<<" condition="<<condition<<" mode="<<mode<<" sample="<<sample
                            <<" owner_cpu_ms="<<cost.cpu[sample]<<" capture_adopt_encode_retire_ms="<<cost.encode[sample]
                            <<" completion_envelope_ms="<<cost.complete[sample]<<" gpu_sum_ms="<<cost.gpu[sample]
                            <<" source_publications="<<cost.publication_samples[sample]<<" requested="<<cost.requested_samples[sample]
                            <<" bound="<<cost.bound_samples[sample]<<" fallback="<<cost.fallback_samples[sample]
                            <<" main_row_publications="<<cost.page_publication_samples[0][sample]
                            <<" probe_row_publications="<<cost.page_publication_samples[1][sample]
                            <<" page_context_may_share=1 page_counters_not_summed=1\n";
                }
            }
        }
    }
}

#include "alpipeline_source_command_cost_native.inl"

TEST_CASE("Actual direct material cohorts include dirty publication raw admission and CPU retirement costs",
          "[almetal][.native-direct-cohort-cost]")
{
    constexpr U32 samples=12,warmups=2;
    const auto cpu_ns=[] {timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
        return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;};
    const auto median=[](auto values){std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    struct Sample
    {
        double cpu=0,encode=0,complete=0,gpu=0;
        U64 publications=0,group_builds=0,source_builds=0,restores=0,getters=0,dependencies=0,images=0;
        U64 raw_bytes=0,ref_bytes=0,order_bytes=0,cohort_bytes=0,cert_bytes=0,native_bytes=0;
        U64 cohort_draws=0,cpu_pins=0,native_pins=0,gpu_ready=0;
        U64 accepted_views=0,accepted_groups=0,accepted_draws=0,static_source_records=0,same_view_replacements=0;
    };
    for(U32 family:{1u,2u})
    {
        MaterialGroupPublication f(family);Inputs::gSavedSettings.native_resources=false;
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);INFO(f.error);REQUIRE(data);
        auto library=data->builtinLibrary(&f.error);INFO(f.error);REQUIRE(library);
        auto main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        auto probes=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        INFO(f.error);REQUIRE(main);REQUIRE(probes);main->setFrameData(data);probes->setFrameData(data);
        U64 native_epoch=0;
        struct Arm {Inputs::LLPipeline pipeline;std::vector<Inputs::LLDrawInfo*> sources;std::vector<ALMetalWorldVisibilityQuery> queries;};
        for(U32 count:{1u,4u,16u,512u})
        {
            std::array<Arm,2> arms;
            for(auto& arm:arms)for(U32 source=0;source<count;++source)arm.sources.push_back(&f.materialDraw());
            for(U32 views:{1u,3u})for(U32 condition=0;condition<3;++condition)
            {
                CAPTURE(family,count,views,condition);
                f.group->invalidate();f.group_layout.reset();f.group_dependencies.reset();
                for(auto& arm:arms)for(auto* source:arm.sources)source->state->invalidate();
                std::array<std::array<Sample,samples>,2> costs;
                for(U32 sample=0;sample<samples+warmups;++sample)
                {
                    ++Inputs::gFrameCount;
                    std::array<std::array<GroupPublication::Image,3>,2> pixels;
                    std::array<std::array<std::array<std::uint64_t,2>,3>,2> queries{};
                    for(U32 position=0;position<2;++position)
                    {
                        const U32 mode=(sample+position)%2;auto& arm=arms[mode];CAPTURE(mode,sample);
                        auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                        const auto before_reads=f.reads();const auto before_serial=Inputs::sNativeSceneSerial;
                        const auto before_stats=Inputs::ALSceneGroupPublicationNative::statistics;
                        const auto before_records=Inputs::ALScenePublicationNative::cohort_source_publications;
                        const auto before_native=data->stats().published_bytes;
                        const std::array<ALMetalWorldCohortStats,2> before_render{main->worldCohortStats(),probes->worldCohortStats()};
                        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                        std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                        Sample value;const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                        data->beginFrame(++native_epoch);
                        if(condition==0)
                        {
                            for(auto* source:arm.sources)source->state->invalidate();
                            if(mode){f.group->invalidate();f.group_layout.reset();f.group_dependencies.reset();}
                        }
                        if(condition==2)
                        {
                            const auto tint=sample%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                            const std::array<LLColor4U,3> colors{tint,tint,tint};
                            for(U32 source=0;source<count;source+=16)
                            {auto& vb=*arm.sources[source]->mVertexBuffer;vb.setColorData(colors.data());vb.unmapBuffer();}
                        }
                        for(U32 view=0;view<views;++view)
                        {
                            capture.clearDraws();capture.complete=true;f.configure(capture,false,false);
                            capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            Inputs::LLCullResult cull;
                            struct Pending {U32 type,raw_begin,count;};
                            std::unordered_map<const void*,std::vector<Pending>> pending;
                            auto packet=mode?f.currentMaterial():std::shared_ptr<const NativeGroup>{};
                            if(packet)REQUIRE(cull.pushNativeMaterialRange(f.type,packet,count,f.region.snapshot()));
                            else for(auto* source:arm.sources)cull.pushDrawInfo(f.type,source);
                            if(mode&&!packet)pending[f.group.get()].push_back({f.type,0,count});
                            // Actual postSort admits raw first, seals after
                            // deferred native stores flush, and replaces the
                            // exact same-view raw span only after whole success.
                            if(mode&&!packet)
                            {
                                auto sealed=Inputs::ALSceneGroupPublicationNative::sealMaterial(f.group,f.group_dependencies,f.group_layout,
                                    f.model,f.region.snapshot(),arm.sources,f.flavor,Inputs::sNativeSceneSources,
                                    Inputs::sNativeSceneSerial,Inputs::gFrameCount);REQUIRE(sealed);
                                value.cert_bytes+=sealed->cohort->metadataBytes();
                                const auto admitted=pending.find(f.group.get());REQUIRE(admitted!=pending.end());
                                const auto span=std::find_if(admitted->second.begin(),admitted->second.end(),[&](const auto& value){return value.type==f.type;});
                                REQUIRE(span!=admitted->second.end());REQUIRE(span->count==arm.sources.size());
                                std::vector<Inputs::LLDrawInfo*> expected(arm.sources.begin(),arm.sources.end());
                                REQUIRE(cull.replaceNativeMaterialRange(f.type,sealed,span->raw_begin,expected,f.region.snapshot()));
                                ++value.same_view_replacements;
                            }
                            {CullScope scope(cull);f.capture(arm.pipeline);}REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==count);
                            REQUIRE(capture.flushDrawOrder());CHECK_FALSE(capture.source_rows);CHECK(capture.source_references.empty());
                            value.raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                            value.ref_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);
                            value.order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);
                            value.cohort_bytes+=capture.source_cohorts.size()*sizeof(ALMetalWorldSourceCohortView);
                            value.cohort_draws+=capture.source_cohort_draw_count;
                            value.cpu_pins+=capture.source_cohorts.size()+(capture.cpu_sources?capture.cpu_sources->owners.size():0);
                            value.native_pins+=capture.source_cohorts.size()+capture.world_resources->owners.size();
                            arm.queries.resize(2);
                            for(U32 query=0;query<2;++query)
                            {
                                auto& q=arm.queries[query];q={};std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
                                q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=.75f;
                                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;q.batch_offset=query?count:count/2;q.fan=7;q.count_samples=1;
                            }
                            auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                            scene.material_argument_buffers=false;scene.source_command_banks=false;
                            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
                                std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(arm.queries),
                                std::move(capture.cpu_sources),capture.world_resources,native_epoch,&f.error);INFO(f.error);REQUIRE(inputs);scene={};
                            auto& renderer=view==0?*main:*probes;
                            REQUIRE(renderer.beginCommandBatch("Actual ordinary/direct-cohort complete cost",&f.error));
                            targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                            completions[view]=renderer.commandBatchCompletion();
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,
                                capture.draw_order,capture.source_cohorts,arm.queries));
                            capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            cull.clear();packet.reset();REQUIRE(renderer.commitCommandBatch(&f.error));
                        }
                        const auto encoded=std::chrono::steady_clock::now();
                        REQUIRE(f.device->waitUntilIdle(&f.error));
                        for(U32 view=0;view<views;++view)for(U32 query=0;query<2;++query)
                        {REQUIRE(results[view]->wait(query,queries[mode][view][query],&f.error));CHECK(queries[mode][view][query]>0);}
                        gGL.frameFences().release();const auto completed=std::chrono::steady_clock::now();
                        value.cpu=(cpu_ns()-begin_cpu)/1e6;value.encode=std::chrono::duration<double,std::milli>(encoded-begin).count();
                        value.complete=std::chrono::duration<double,std::milli>(completed-begin).count();
                        for(U32 view=0;view<views;++view)
                        {ALMetalCommandGPUTime time;if(completions[view]&&completions[view]->gpuTime(time)==ALMetalCommandTimingStatus::ready)
                            {value.gpu+=time.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                        const auto after_reads=f.reads();const auto after_stats=Inputs::ALSceneGroupPublicationNative::statistics;
                        value.publications=Inputs::sNativeSceneSerial-before_serial;value.group_builds=after_stats.material_builds-before_stats.material_builds;
                        value.source_builds=after_stats.cpu_source_builds-before_stats.cpu_source_builds;value.restores=after_stats.material_restores-before_stats.material_restores;
                        value.getters=after_reads[0]-before_reads[0];value.dependencies=after_reads[1]-before_reads[1];value.images=after_reads[2]-before_reads[2];
                        value.native_bytes=data->stats().published_bytes-before_native;
                        value.static_source_records=Inputs::ALScenePublicationNative::cohort_source_publications-before_records;
                        const std::array<ALMetalWorldCohortStats,2> after_render{main->worldCohortStats(),probes->worldCohortStats()};
                        for(U32 owner=0;owner<2;++owner)
                        {value.accepted_views+=after_render[owner].views-before_render[owner].views;
                         value.accepted_groups+=after_render[owner].groups-before_render[owner].groups;
                         value.accepted_draws+=after_render[owner].draws-before_render[owner].draws;}
                        if(sample>=warmups)
                        {
                            const U64 updated=condition==0?count:condition==1?0:(count+15)/16;
                            CHECK(value.group_builds==(mode&&condition!=1?1:0));
                            CHECK(value.source_builds==(mode?updated:0));
                            CHECK(value.publications==updated+value.group_builds);
                            CHECK(value.static_source_records==(mode?updated:0));
                            CHECK(value.cohort_draws==(mode?U64(count)*views:0));
                            CHECK(value.accepted_draws==value.cohort_draws);
                            CHECK(value.accepted_groups==(mode?views:0));
                            CHECK(value.accepted_views==value.accepted_groups);
                            CHECK(value.same_view_replacements==(mode&&condition!=1?1:0));
                            if(mode&&condition==1){CHECK(value.getters==0);CHECK(value.dependencies==0);CHECK(value.images==0);}
                            costs[mode][sample-warmups]=value;
                        }
                        if(sample==warmups-1)for(U32 view=0;view<views;++view)pixels[mode][view]=f.read(*targets[view]);
                        std::cout<<"NATIVE_DIRECT_COHORT_RAW family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                            <<" condition="<<condition<<" mode="<<mode<<" sample="<<sample<<" warmup="<<(sample<warmups)
                            <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" completion_and_query_settle_ms="<<value.complete
                            <<" gpu_sum_ms="<<value.gpu<<" gpu_ready_commands="<<value.gpu_ready<<" source_and_group_publications="<<value.publications
                            <<" group_builds="<<value.group_builds<<" source_factory_builds="<<value.source_builds<<" source_restores="<<value.restores
                            <<" new_static_source_records="<<value.static_source_records
                            <<" same_view_replacements="<<value.same_view_replacements
                            <<" source_getters="<<value.getters<<" dependency_walks="<<value.dependencies<<" image_getters="<<value.images
                            <<" direct_cohort_draws="<<value.cohort_draws<<" raw_value_bytes="<<value.raw_bytes<<" reference_value_bytes="<<value.ref_bytes
                            <<" accepted_cohort_views="<<value.accepted_views<<" accepted_cohort_groups="<<value.accepted_groups<<" accepted_cohort_draws="<<value.accepted_draws
                            <<" order_value_bytes="<<value.order_bytes<<" cohort_value_bytes="<<value.cohort_bytes<<" dirty_certificate_bytes="<<value.cert_bytes
                            <<" native_frame_bytes="<<value.native_bytes<<" cpu_description_pins="<<value.cpu_pins<<" native_generation_pins="<<value.native_pins<<'\n';
                    }
                    if(sample==warmups-1)for(U32 view=0;view<views;++view)CHECK(pixels[0][view]==pixels[1][view]);
                    for(U32 view=0;view<views;++view)CHECK(queries[0][view]==queries[1][view]);
                }
                for(U32 mode=0;mode<2;++mode)
                {
                    std::array<double,samples> cpu{},encode{},complete{},gpu{};
                    for(U32 sample=0;sample<samples;++sample){const auto& value=costs[mode][sample];cpu[sample]=value.cpu;encode[sample]=value.encode;complete[sample]=value.complete;gpu[sample]=value.gpu;}
                    std::cout<<"NATIVE_DIRECT_COHORT_COST family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                        <<" condition="<<(condition==0?"cold":condition==1?"warm":"churn")
                        <<" mode="<<(mode?"direct_cohort":"ordinary_native")<<" samples="<<samples<<" warmups="<<warmups
                        <<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                        <<" capture_adopt_encode_reclaim_median_ms="<<median(encode)<<" capture_adopt_encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                        <<" completion_and_query_settle_median_ms="<<median(complete)<<" completion_and_query_settle_max_ms="<<*std::max_element(complete.begin(),complete.end())
                        <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                        <<" update_stride=16 paired_logical_frame=1 independent_native_arm_epochs=1 shared_exact_library=1"
                        <<" dirty_view_raw_admission_before_seal=1 same_view_verified_replacement=1 source_rows=0 command_banks=0 resource_table_flag=0"
                        <<" geometry_texture_drawinfo_creation_excluded=1 viewer_dependency_frontier=typed_native_signals whole_viewer_fps_claim=0\n";
                }
            }
        }
    }
}
