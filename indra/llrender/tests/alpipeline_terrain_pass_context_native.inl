/** Actual terrain pass-owner body with explicit region/camera/map inputs. */
namespace
{
namespace TerrainPassContextTest
{
struct LLViewerCamera
{
    LLMatrix4a view,projection;
    LLViewerCamera(){view.setIdentity();projection.setIdentity();}
    static LLViewerCamera& getCurrent(){static LLViewerCamera value;return value;}
    const LLMatrix4a& getModelview() const{return view;}
    const LLMatrix4a& getProjection() const{return projection;}
};
struct LLViewerRegion
{
    ALNativeRegionModel model;
    ALNativeRegionModel::Snapshot nativeModelSnapshot() const{return model.snapshot();}
};
struct ALTerrainSurfaceMaps
{
    std::shared_ptr<const ALMetalTexture> height,composition;
    auto getMetalHeightTextureLease() const{return height;}
    auto getMetalCompositionTextureLease() const{return composition;}
};
struct ALMetalTerrainFetchedBindings
{
    struct Binding{Inputs::Ref<LLImageGL> image;};
    std::array<Binding,4> base,normal,orm,emissive;
    Binding blend,overlay;
};
#include "../../newview/pipeline_terrain_pass_capture_native.inl"

struct ContextFixture
{
    Publication f;
    ALTerrainSurfaceMaps maps;
    std::shared_ptr<ALMetalBuffer> geometry;
    ALMetalWorldSimpleBatch patch;
    ContextFixture()
    {
        LLViewerCamera::getCurrent()=LLViewerCamera{};
        const std::array<float,16> corners{0,0,0,1,2,0,0,1,2,2,0,1,0,2,0,1};
        ALMetalBufferDescriptor buffer;buffer.length=sizeof(corners);buffer.storage=ALMetalStorageMode::shared;
        geometry=ALMetalBuffer::create(*f.device,buffer,&f.error);REQUIRE(geometry);
        REQUIRE(geometry->write(0,corners.data(),sizeof(corners),&f.error));
        const auto make=[&](ALMetalPixelFormat format)
        {
            ALMetalTextureDescriptor texture;texture.width=texture.height=8;texture.format=format;
            texture.usage=ALMetalTextureUsage::shader_read;
            std::shared_ptr<const ALMetalTexture> result=ALMetalTexture::create(*f.device,texture,&f.error);
            INFO(f.error);REQUIRE(result);return result;
        };
        maps.height=make(ALMetalPixelFormat::r32_float);maps.composition=make(ALMetalPixelFormat::rg16_float);
        patch.terrain=true;patch.vertices=geometry.get();patch.vertex_count=4;patch.texture_count=4;
        std::fill_n(patch.diffuse_textures.begin(),4,f.image.getMetalTexture());
        patch.terrain_height_texture=maps.height.get();patch.terrain_composition_texture=maps.composition.get();
        patch.terrain_alpha_ramp_texture=f.image.getMetalTexture();
        LLMatrix4a identity;identity.setIdentity();std::memcpy(patch.modelview_projection,identity.getF32ptr(),64);
        std::memcpy(patch.terrain_modelview,identity.getF32ptr(),64);
        for(U32 axis=0;axis<3;++axis)patch.normal_matrix[axis][axis]=1;
        std::fill_n(patch.terrain_tessellation_factors,6,std::uint16_t{0x3c00});
    }
    std::shared_ptr<const ALMetalWorldTerrainPass> seal()
    {
        LLImageGL* images[]{&f.image};return ALMetalWorldTerrainPassCapture::seal(patch,maps,nullptr,images,1);
    }
};
}
}

TEST_CASE("Actual terrain pass context separates region revision, camera and family", "[pipeline-buffer-publication][terrain-pass-context]")
{
    using namespace TerrainPassContextTest;ContextFixture f;
    auto pass=f.seal();INFO(f.f.error);REQUIRE(pass);
    LLViewerRegion region;LLMatrix4a model;model.setIdentity();REQUIRE(region.model.update(model,LLVector3::zero));
    ALMetalWorldTerrainPassCapture context;CHECK_FALSE(context.current(region,false));
    context.remember(region,false,pass);CHECK(context.current(region,false));CHECK_FALSE(context.current(region,true));
    auto& camera=LLViewerCamera::getCurrent();const auto original=camera.view;
    camera.view.mMatrix[3].set(.25f,0.f,0.f,1.f);CHECK_FALSE(context.current(region,false));
    camera.view=original;CHECK(context.current(region,false));
    camera.projection.mMatrix[0].set(.75f,0.f,0.f,0.f);CHECK_FALSE(context.current(region,false));
    camera.projection.setIdentity();CHECK(context.current(region,false));
    model.mMatrix[3].set(.5f,0.f,0.f,1.f);REQUIRE(region.model.update(model,LLVector3(.5f,0.f,0.f)));
    CHECK_FALSE(context.current(region,false));
    LLViewerRegion other;REQUIRE(other.model.update(model,LLVector3(.5f,0.f,0.f)));CHECK_FALSE(context.current(other,false));
}

TEST_CASE("Actual terrain pass seal selects exact map and fetched texture leases without geometry ownership", "[pipeline-buffer-publication][terrain-pass-context]")
{
    using namespace TerrainPassContextTest;ContextFixture f;
    CHECK_FALSE(ALMetalWorldTerrainPassCapture::seal(f.patch,f.maps,nullptr,nullptr,0));
    ALMetalTerrainFetchedBindings fetched;fetched.base[0].image.value=&f.f.image;
    auto pass=ALMetalWorldTerrainPassCapture::seal(f.patch,f.maps,&fetched,nullptr,0);REQUIRE(pass);
    pass->install(f.patch);CHECK(pass->matches(f.patch));
    const auto old_height=f.patch.terrain_height_texture;auto old=f.maps.height;
    ALMetalTextureDescriptor next;next.width=next.height=8;next.format=ALMetalPixelFormat::r32_float;
    next.usage=ALMetalTextureUsage::shader_read;
    f.maps.height=ALMetalTexture::create(*f.f.device,next,&f.f.error);REQUIRE(f.maps.height);
    CHECK(pass->matches(f.patch));CHECK(f.patch.terrain_height_texture==old_height);
    // The old patch is still individually valid, but its exact height lease
    // is absent from the new maps/fetched tuple. Format validation cannot
    // explain this rejection: both height generations are valid R32 textures.
    CHECK_FALSE(ALMetalWorldTerrainPassCapture::seal(f.patch,f.maps,&fetched,nullptr,0));
    auto changed=f.patch;changed.terrain_height_texture=f.maps.height.get();CHECK_FALSE(pass->matches(changed));
    CHECK(ALMetalWorldTerrainPassCapture::seal(changed,f.maps,&fetched,nullptr,0));
    std::weak_ptr<const ALMetalBuffer> weak=f.geometry;f.geometry.reset();CHECK(weak.expired());
    // No native draw is attempted after geometry retirement. Only CPU pass
    // lifetime and exact selected texture owners are under test here.
    CHECK(pass->resourceLease());
}
