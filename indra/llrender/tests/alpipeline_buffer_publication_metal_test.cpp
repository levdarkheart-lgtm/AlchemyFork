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
#include "../../almetal/almetalshadowprepare.h"
#include "../../almetal/almetalworldvisibility.h"
#include "alnativesourceworkingset.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
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
    LLImageGL* getGLTexture() const{return image;}
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
    ~LLDrawInfo(){state->invalidate();}
    const std::shared_ptr<ALNativePublication::State>& metalSourcePublication() const{return state;}
    void observeMetalSourceDependencies() const
    {
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
struct Settings {bool native_visibility=false;} gSavedSettings;
template<class T> struct LLCachedControl
{
    Settings& settings;
    LLCachedControl(Settings& source,const char*,T):settings(source){}
    T operator()() const{return static_cast<T>(settings.native_visibility);}
};
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
