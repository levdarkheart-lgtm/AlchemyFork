/**
 * @file almetalworldcohort.mm
 * @brief Dirty-only ordinary material proof; weak producer ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldcohort.h"
#include "almetalworldprepare.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace
{
bool finite(const float* values,std::size_t count) noexcept
{for(std::size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
// Compare only source-owned words copied into WorldSimpleConstants. Every
// row of one cohort uses the same exact view matrices and scene clip/flags.
// This runs at cold publication, never while encoding a warm view.
bool sameScalarCommon(const ALMetalWorldSimpleBatch& a,const ALMetalWorldSimpleBatch& b) noexcept
{
    const auto equal=[](const auto& x,const auto& y){return std::memcmp(&x,&y,sizeof(x))==0;};
    return equal(a.texture_matrix,b.texture_matrix) && equal(a.avatar_wind,b.avatar_wind) &&
        equal(a.avatar_sin_wave,b.avatar_sin_wave) && equal(a.avatar_gravity,b.avatar_gravity) &&
        equal(a.avatar_cloth_depth,b.avatar_cloth_depth) && equal(a.alpha_cutoff,b.alpha_cutoff) &&
        a.use_vertex_color==b.use_vertex_color && a.alpha_cutoff_bias==b.alpha_cutoff_bias &&
        a.alpha_mask==b.alpha_mask && a.texture_count==b.texture_count;
}
bool exactOwner(const std::weak_ptr<const void>& expected,const void* pointer,
                const std::shared_ptr<const void>& value) noexcept
{return value && value.get()==pointer && !expected.owner_before(value) && !value.owner_before(expected);}
}
struct ALMetalWorldCohortSource::Impl
{
    ALMetalWorldSourceKey key;
    ALMetalWorldSimpleBatch prototype;
    std::uint32_t family=0;
    bool double_sided=false,normal_map=false,specular_map=false;
    bool bounds_published=false;
    ALMetalWorldConservativeBounds bounds;
};
// One shared allocation contains the immutable snapshot and its public
// facade. The alias returned by seal owns Storage, not a parent publication.
struct ALMetalWorldCohortSource::Storage
{
    Impl value;
    ALMetalWorldCohortSource certificate;
    Storage(const ALMetalWorldKeyedSource& source,std::uint32_t family,
        const ALMetalWorldConservativeBounds* bounds)
        :value{source.key,*source.prototype,family,source.prototype->double_sided},certificate(&value)
    {
        if(family==1)
        {
            const auto& slot=source.prototype->materialSlots()[0];
            value.normal_map=slot.normal_texture!=nullptr;value.specular_map=slot.specular_texture!=nullptr;
        }
        if(bounds){value.bounds=*bounds;value.bounds_published=true;}
    }
};
ALMetalWorldCohortSource::ALMetalWorldCohortSource(const Impl* impl) noexcept:mImpl(impl){}
ALMetalWorldCohortSource::~ALMetalWorldCohortSource()=default;
const ALMetalWorldSourceKey& ALMetalWorldCohortSource::key() const noexcept{return mImpl->key;}
const ALMetalWorldSimpleBatch& ALMetalWorldCohortSource::source() const noexcept{return mImpl->prototype;}
std::uint32_t ALMetalWorldCohortSource::family() const noexcept{return mImpl->family;}
bool ALMetalWorldCohortSource::doubleSided() const noexcept{return mImpl->double_sided;}
ALMetalWorldScalarRecipe ALMetalWorldCohortSource::scalarRecipe() const noexcept
{return {static_cast<std::uint8_t>(mImpl->family),mImpl->normal_map,mImpl->specular_map};}
bool ALMetalWorldCohortSource::hasBoundsPublication() const noexcept{return mImpl->bounds_published;}
const ALMetalWorldConservativeBounds& ALMetalWorldCohortSource::bounds() const noexcept{return mImpl->bounds;}
std::size_t ALMetalWorldCohortSource::ownedBytes() const noexcept{return sizeof(Storage);}
std::shared_ptr<const ALMetalWorldCohortSource> ALMetalWorldCohortSource::seal(
    const ALMetalWorldKeyedSource& keyed,std::string* error,const ALMetalWorldConservativeBounds* bounds)
try
{
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldCohortSource>
    {if(error)*error=message;return {};};
    if(!keyed.prototype || !keyed.key.identity || !keyed.key.revision)
        return reject("Ordinary material source identity is incomplete");
    const auto& batch=*keyed.prototype;
    const std::uint32_t family=batch.material_slot_count?1u:(batch.pbr_slot_count?2u:0u);
    if((keyed.key.flavor&3u)!=family || batch.normal_texture || batch.skin_palette_record || batch.skin_palette ||
       batch.skin_palette_float_count || batch.skin_joint_count || batch.weight_offset || batch.cloth_offset ||
       batch.avatar_rigid || batch.avatar_skinning || batch.avatar_cloth || batch.terrain || batch.terrain_pass || batch.impostor ||
       batch.impostor_fullbright || batch.impostor_shiny || batch.impostor_emboss)
        return reject("Ordinary material source is unsupported or unsealed");
    if(family)
    {
        if(!batch.native_material_source || !batch.native_material_source->matches(batch) ||
           (batch.native_resource_source && !batch.native_resource_source->matches(batch)))
            return reject("Ordinary material source is unsupported or unsealed");
    }
    else if(batch.native_material_source || batch.native_resource_source ||
            batch.impostor_texture_owner || batch.terrain_texture_owner || batch.avatar_alpha || batch.alpha_mask ||
            batch.water_exclusion || batch.water_haze || batch.water_surface || batch.atmospheric_haze ||
            batch.pre_water_alpha || batch.post_water_alpha || batch.post_water_depth_only ||
            batch.post_water_glow || batch.legacy_glow || batch.pbr_glow)
        return reject("Ordinary SIMPLE source contains owning or special-pass state");
    ALMetalWorldViewMatrices canonical;
    for(unsigned axis=0;axis<4;++axis)
        canonical.modelview_projection[axis*5]=canonical.modelview[axis*5]=1.f;
    for(unsigned axis=0;axis<3;++axis)
        canonical.normal_matrix[axis][axis]=canonical.rigged_normal_matrix[axis][axis]=1.f;
    ALMetalWorldGBufferScene validation;validation.width=validation.height=1;
    const ALMetalWorldDraw draw(batch,&canonical,batch.texture_matrix);
    // Preserve both ordinary normal-target ABIs. The old prototype camera
    // is unused; its fixed source UV and all static native ranges are checked.
    for(bool normal_hdr:{false,true})
    {
        validation.normal_hdr=normal_hdr;
        if(!alMetalValidateWorldBatch(validation,draw,error))return {};
    }
    auto storage=std::make_shared<Storage>(keyed,family,bounds);
    return std::shared_ptr<const ALMetalWorldCohortSource>(storage,&storage->certificate);
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

struct ALMetalWorldSourceCohort::Impl
{
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> sources;
    std::vector<std::uint8_t> scalar_common_previous;
    std::weak_ptr<const void> cpu,native;
    const void* cpu_pointer=nullptr;
    const void* native_pointer=nullptr;
    std::uint64_t region=0;
    std::uint32_t family=0;
    bool family_initialized=false,double_sided=false,uniform_double_sided=true;
    std::size_t qualified_bounds=0;
    ALMetalWorldSourceCohort::NativeTextureResolver resolve_texture=nullptr;
};
ALMetalWorldSourceCohort::ALMetalWorldSourceCohort(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceCohort::~ALMetalWorldSourceCohort()=default;
std::size_t ALMetalWorldSourceCohort::size() const noexcept{return mImpl->sources.size();}
std::uint32_t ALMetalWorldSourceCohort::family() const noexcept{return mImpl->family;}
bool ALMetalWorldSourceCohort::doubleSided() const noexcept{return mImpl->double_sided;}
bool ALMetalWorldSourceCohort::uniformDoubleSided() const noexcept{return mImpl->uniform_double_sided;}
std::size_t ALMetalWorldSourceCohort::metadataBytes() const noexcept
{
    std::size_t total=sizeof(*this)+sizeof(Impl)+mImpl->sources.capacity()*sizeof(mImpl->sources[0])+
        mImpl->scalar_common_previous.capacity()*sizeof(mImpl->scalar_common_previous[0]);
    for(const auto& source:mImpl->sources)
    {
        if(source->ownedBytes()>std::numeric_limits<std::size_t>::max()-total)return std::numeric_limits<std::size_t>::max();
        total+=source->ownedBytes();
    }
    return total;
}
std::size_t ALMetalWorldSourceCohort::ownedBytes() const noexcept{return metadataBytes();}
std::uint64_t ALMetalWorldSourceCohort::regionIdentity() const noexcept{return mImpl->region;}
const ALMetalWorldSimpleBatch& ALMetalWorldSourceCohort::source(std::size_t index) const noexcept
{return mImpl->sources[index]->source();}
const ALMetalWorldSourceKey& ALMetalWorldSourceCohort::key(std::size_t index) const noexcept
{return mImpl->sources[index]->key();}
ALMetalWorldScalarRecipe ALMetalWorldSourceCohort::scalarRecipe(std::size_t index) const noexcept
{return mImpl->sources[index]->scalarRecipe();}
bool ALMetalWorldSourceCohort::sameScalarCommonAsPrevious(std::size_t index) const noexcept
{return index<mImpl->scalar_common_previous.size() && mImpl->scalar_common_previous[index]!=0;}
std::size_t ALMetalWorldSourceCohort::scalarCommonMetadataBytes() const noexcept
{return mImpl->scalar_common_previous.capacity()*sizeof(mImpl->scalar_common_previous[0]);}
const ALMetalWorldConservativeBounds& ALMetalWorldSourceCohort::bounds(std::size_t index) const noexcept
{return mImpl->sources[index]->bounds();}
std::size_t ALMetalWorldSourceCohort::qualifiedBoundsCount() const noexcept{return mImpl->qualified_bounds;}
bool ALMetalWorldSourceCohort::matchesOwners(const std::shared_ptr<const void>& cpu,
                                           const std::shared_ptr<const void>& native) const noexcept
{return exactOwner(mImpl->cpu,mImpl->cpu_pointer,cpu) && exactOwner(mImpl->native,mImpl->native_pointer,native);}
std::shared_ptr<const ALMetalTexture> ALMetalWorldSourceCohort::resolveNativeTexture(
    const std::shared_ptr<const void>& native,const ALMetalTexture* texture) const noexcept
{
    if(!texture || !mImpl->resolve_texture || !exactOwner(mImpl->native,mImpl->native_pointer,native))return {};
    auto lease=mImpl->resolve_texture(native,texture);
    if(!lease || lease.get()!=texture || (!lease.owner_before(native) && !native.owner_before(lease)))return {};
    return lease;
}
bool ALMetalWorldSourceCohort::validView(const ALMetalWorldViewMatrices& view) noexcept
{
    // The texture matrix belongs to each sealed source, not the shared view.
    return finite(view.modelview_projection,16) && finite(view.modelview,16) && view.modelview[15]==1.f &&
        finite(&view.normal_matrix[0][0],12) && finite(&view.rigged_normal_matrix[0][0],12);
}
std::shared_ptr<const ALMetalWorldSourceCohort> ALMetalWorldSourceCohort::seal(
    std::span<const ALMetalWorldKeyedSource> sources,std::shared_ptr<const void> cpu,
    std::shared_ptr<const void> native,std::uint64_t region,std::string* error,NativeTextureResolver resolver)
try
{
    if(sources.empty() || !cpu || !native || !region)
    {if(error)*error="Ordinary material cohort requires completed immutable producer ownership";return {};}
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;records.reserve(sources.size());
    for(const auto& keyed:sources)
    {
        auto source=ALMetalWorldCohortSource::seal(keyed,error);if(!source)return {};
        records.push_back(std::move(source));
    }
    return seal(records,std::move(cpu),std::move(native),region,error,resolver);
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

std::shared_ptr<const ALMetalWorldSourceCohort> ALMetalWorldSourceCohort::seal(
    std::span<const std::shared_ptr<const ALMetalWorldCohortSource>> sources,std::shared_ptr<const void> cpu,
    std::shared_ptr<const void> native,std::uint64_t region,std::string* error,NativeTextureResolver resolver)
try
{
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldSourceCohort>
    {if(error)*error=message;return {};};
    if(sources.empty() || !cpu || !native || !region)
        return reject("Ordinary material cohort requires completed immutable producer ownership");
    auto impl=std::make_unique<Impl>();
    impl->sources.reserve(sources.size());
    impl->scalar_common_previous.reserve(sources.size());
    for(const auto& source:sources)
    {
        if(!source || (impl->family_initialized && impl->family!=source->family()))
            return reject("Ordinary material cohort contains missing or mixed-family records");
        if(!impl->family_initialized)
        {impl->family=source->family();impl->double_sided=source->doubleSided();impl->family_initialized=true;}
        else impl->uniform_double_sided=impl->uniform_double_sided && impl->double_sided==source->doubleSided();
        impl->scalar_common_previous.push_back(!impl->sources.empty() &&
            sameScalarCommon(impl->sources.back()->source(),source->source()));
        impl->sources.push_back(source);
        const auto& bounds=source->bounds();
        if(bounds.qualified && finite(bounds.minimum,4) && finite(bounds.maximum,4) &&
           bounds.minimum[0]<=bounds.maximum[0] && bounds.minimum[1]<=bounds.maximum[1] && bounds.minimum[2]<=bounds.maximum[2])
            ++impl->qualified_bounds;
    }
    // As with NativeScope/SourceReference, the completed producer is
    // responsible for the typed native union. A void owner cannot be
    // introspected to establish its contents. The actual group producer
    // verifies that union before issuing this immutable certificate.
    impl->cpu=cpu;impl->native=native;
    impl->cpu_pointer=cpu.get();impl->native_pointer=native.get();impl->region=region;impl->resolve_texture=resolver;
    return std::shared_ptr<const ALMetalWorldSourceCohort>(new ALMetalWorldSourceCohort(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}
