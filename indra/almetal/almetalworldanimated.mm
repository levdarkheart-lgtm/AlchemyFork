/** Native immutable regular-rigged source publication. */
#include "almetalworldanimated.h"
#include "almetalworld.h"
#include "almetalworldprepare.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace
{
bool finiteValue(float value) noexcept
{return (std::bit_cast<std::uint32_t>(value)&0x7f800000u)!=0x7f800000u;}
bool exactOwner(const std::weak_ptr<const void>& expected,const void* pointer,
                const std::shared_ptr<const void>& owner) noexcept
{return owner && owner.get()==pointer && !expected.owner_before(owner) && !owner.owner_before(expected);}
}
struct ALMetalWorldAnimatedSource::Impl
{
    ALMetalWorldSourceKey key;
    ALMetalWorldSimpleBatch prototype;
    std::vector<ALMetalWorldAnimatedBoneBounds> bounds;
    std::weak_ptr<const void> native;
    const void* native_pointer=nullptr;
    std::weak_ptr<const ALMetalWorldAnimatedSource> owner;
    ALMetalWorldScalarRecipe recipe;
    bool qualified=false,gpu_ready=false;
};
ALMetalWorldAnimatedSource::ALMetalWorldAnimatedSource(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldAnimatedSource::~ALMetalWorldAnimatedSource()=default;
const ALMetalWorldSimpleBatch& ALMetalWorldAnimatedSource::prototype() const noexcept{return mImpl->prototype;}
const ALMetalWorldSourceKey& ALMetalWorldAnimatedSource::key() const noexcept{return mImpl->key;}
std::shared_ptr<const ALMetalWorldAnimatedSource> ALMetalWorldAnimatedSource::owner() const noexcept{return mImpl->owner.lock();}
std::shared_ptr<const void> ALMetalWorldAnimatedSource::nativeLease() const noexcept{return mImpl->native.lock();}
bool ALMetalWorldAnimatedSource::matchesNativeScope(const ALMetalWorldSourceNativeScope& scope) const noexcept
{return exactOwner(mImpl->native,mImpl->native_pointer,scope.owner) &&
    (!scope.vertices || scope.vertices.get()==mImpl->prototype.vertices) &&
    (!scope.indices || scope.indices.get()==mImpl->prototype.indices);}
std::span<const ALMetalWorldAnimatedBoneBounds> ALMetalWorldAnimatedSource::boneBounds() const noexcept{return mImpl->bounds;}
bool ALMetalWorldAnimatedSource::boundsQualified() const noexcept{return mImpl->qualified;}
bool ALMetalWorldAnimatedSource::gpuReady() const noexcept{return mImpl->gpu_ready;}
ALMetalWorldScalarRecipe ALMetalWorldAnimatedSource::scalarRecipe() const noexcept{return mImpl->recipe;}
bool ALMetalWorldAnimatedSource::sameScalarCommon(const ALMetalWorldAnimatedSource& other)const noexcept
{
    if(this==&other)return true;
    const auto& a=mImpl->prototype;const auto& b=other.mImpl->prototype;
    const auto equal=[](const auto& x,const auto& y){return std::memcmp(&x,&y,sizeof(x))==0;};
    return equal(a.avatar_wind,b.avatar_wind) && equal(a.avatar_sin_wave,b.avatar_sin_wave) &&
        equal(a.avatar_gravity,b.avatar_gravity) && equal(a.avatar_cloth_depth,b.avatar_cloth_depth) &&
        equal(a.alpha_cutoff,b.alpha_cutoff) && a.use_vertex_color==b.use_vertex_color &&
        a.alpha_cutoff_bias==b.alpha_cutoff_bias && a.alpha_mask==b.alpha_mask && a.texture_count==b.texture_count;
}
std::size_t ALMetalWorldAnimatedSource::ownedBytes() const noexcept
{return sizeof(*this)+sizeof(Impl)+mImpl->bounds.capacity()*sizeof(ALMetalWorldAnimatedBoneBounds);}

std::shared_ptr<const ALMetalWorldAnimatedSource> ALMetalWorldAnimatedSource::seal(
    ALMetalWorldSourceKey key,const ALMetalWorldSimpleBatch& source,
    const ALMetalWorldSourceNativeScope& native,std::string* error,bool require_gpu)
try
{
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldAnimatedSource>
    {if(error)*error=message;return {};};
    const bool legacy=source.material_slot_count!=0,pbr=source.pbr_slot_count!=0;
    if(!key.identity || !key.revision || !(key.flavor&(1ull<<7)) || legacy==pbr ||
       (key.flavor&3)!=(pbr?2u:1u) || !native.owner || !source.vertices || !source.indices ||
       (native.vertices && native.vertices.get()!=source.vertices) ||
       (native.indices && native.indices.get()!=source.indices) ||
       source.skin_palette || source.skin_palette_record || source.body_palette_record ||
       source.skin_joint_count || source.skin_palette_float_count || source.avatar_rigid || source.avatar_skinning ||
       source.avatar_cloth || source.avatar_alpha || source.cloth_offset || source.terrain || source.terrain_pass ||
       source.impostor || source.impostor_texture_owner || source.terrain_texture_owner || source.normal_texture ||
       source.pre_water_alpha || source.post_water_alpha || source.post_water_depth_only || source.post_water_glow ||
       source.legacy_glow || source.pbr_glow || source.water_exclusion || source.water_haze || source.water_surface ||
       source.atmospheric_haze || source.impostor_fullbright || source.impostor_shiny || source.impostor_emboss ||
       !source.native_material_source || !source.native_material_source->matches(source) ||
       (require_gpu && !source.native_resource_source) ||
       (source.native_resource_source && !source.native_resource_source->matches(source)))
        return reject("Animated source requires completed regular rigged geometry/material generations");

    // Ordinary cold validation remains authoritative, using a canonical pose
    // only to prove the static weight4/range/material layout. Never retain it.
    auto validation=source;std::array<float,4+110*12> identity_pose{};
    for(unsigned joint=0;joint<110;++joint)
        for(unsigned axis=0;axis<3;++axis)identity_pose[4+joint*12+axis*4+axis]=1;
    validation.skin_palette=identity_pose.data();validation.skin_joint_count=110;
    validation.skin_palette_float_count=identity_pose.size();
    ALMetalWorldViewMatrices matrices;ALMetalWorldGBufferScene scene;scene.width=scene.height=1;
    for(unsigned axis=0;axis<4;++axis)matrices.modelview_projection[axis*5]=matrices.modelview[axis*5]=1;
    for(unsigned axis=0;axis<3;++axis)matrices.normal_matrix[axis][axis]=matrices.rigged_normal_matrix[axis][axis]=1;
    std::memcpy(validation.modelview_projection,matrices.modelview_projection,64);
    std::memcpy(validation.modelview,matrices.modelview,64);
    std::memcpy(validation.normal_matrix,matrices.normal_matrix,48);
    std::memcpy(validation.rigged_normal_matrix,matrices.rigged_normal_matrix,48);
    for(bool hdr:{false,true})
    {scene.normal_hdr=hdr;if(!alMetalValidateWorldBatch(scene,ALMetalWorldDraw(validation),error))return {};}
    auto impl=std::make_unique<Impl>();impl->key=key;impl->prototype=source;
    impl->native=native.owner;impl->native_pointer=native.owner.get();
    impl->recipe.family=pbr?2:1;
    if(legacy)
    {
        impl->recipe.normal_map=source.materialSlots()[0].normal_texture!=nullptr;
        impl->recipe.specular_map=source.materialSlots()[0].specular_texture!=nullptr;
    }
    if(!require_gpu)
    {
        auto result=std::shared_ptr<const ALMetalWorldAnimatedSource>(new ALMetalWorldAnimatedSource(std::move(impl)));
        result->mImpl->owner=result;return result;
    }

    // Cold scan over the exact indexed native generation, never LL objects.
    // Unreadable/unproved data is legal rendering input but cannot be culled.
    const auto* vertices=static_cast<const std::byte*>(source.vertices->mappedBytes());
    const auto* indices=static_cast<const std::byte*>(source.indices->mappedBytes());
    std::array<ALMetalWorldAnimatedBoneBounds,110> boxes;
    std::array<bool,110> used{};bool qualified=vertices && indices;
    for(auto& box:boxes)for(unsigned axis=0;axis<3;++axis)
    {box.minimum[axis]=std::numeric_limits<float>::infinity();box.maximum[axis]=-std::numeric_limits<float>::infinity();}
    const auto stride=source.index_type==ALMetalIndexType::uint32?4u:2u;
    for(std::uint32_t at=0;qualified && at<source.index_count;++at)
    {
        std::uint32_t index=0;
        if(stride==4)std::memcpy(&index,indices+source.index_offset+std::size_t(at)*4,4);
        else{std::uint16_t small;std::memcpy(&small,indices+source.index_offset+std::size_t(at)*2,2);index=small;}
        if(index>=source.vertex_count){qualified=false;break;}
        float point[4],weights[4];
        std::memcpy(point,vertices+source.position_offset+std::size_t(index)*16,16);
        std::memcpy(weights,vertices+source.weight_offset+std::size_t(index)*16,16);
        for(unsigned axis=0;axis<3;++axis)qualified=qualified && finiteValue(point[axis]);
        float sum=0;std::array<float,4> fractions{};
        for(unsigned influence=0;influence<4;++influence)
        {
            if(!finiteValue(weights[influence]) ||
               (weights[influence]!=0.f && std::fpclassify(weights[influence])==FP_SUBNORMAL))
            {qualified=false;break;}
            fractions[influence]=weights[influence]-std::floor(weights[influence]);sum+=fractions[influence];
        }
        if(!qualified)break;
        for(unsigned influence=0;influence<4;++influence)
        {
            if(sum>0 ? fractions[influence]<=0 : influence!=0)continue;
            const auto joint=static_cast<unsigned>(std::clamp(std::floor(weights[influence]),0.f,109.f));
            used[joint]=true;
            for(unsigned axis=0;axis<3;++axis)
            {boxes[joint].minimum[axis]=std::min(boxes[joint].minimum[axis],point[axis]);
             boxes[joint].maximum[axis]=std::max(boxes[joint].maximum[axis],point[axis]);}
        }
    }
    if(qualified)for(unsigned joint=0;joint<110;++joint)if(used[joint])
    {boxes[joint].config[0]=joint;impl->bounds.push_back(boxes[joint]);}
    impl->qualified=qualified && !impl->bounds.empty();
    impl->gpu_ready=true;
    auto result=std::shared_ptr<const ALMetalWorldAnimatedSource>(new ALMetalWorldAnimatedSource(std::move(impl)));
    result->mImpl->owner=result;return result;
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}
