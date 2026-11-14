/**
 * @file almetalworldcohort.mm
 * @brief Dirty-only ordinary material proof; weak producer ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldcohort.h"
#include "almetalworldprepare.h"
#include <cmath>
#include <new>

namespace
{
bool finite(const float* values,std::size_t count) noexcept
{for(std::size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
bool exactOwner(const std::weak_ptr<const void>& expected,const void* pointer,
                const std::shared_ptr<const void>& value) noexcept
{return value && value.get()==pointer && !expected.owner_before(value) && !value.owner_before(expected);}
}
struct ALMetalWorldSourceCohort::Impl
{
    struct Source {ALMetalWorldSourceKey key;ALMetalWorldSimpleBatch prototype;};
    std::vector<Source> sources;
    std::weak_ptr<const void> cpu,native;
    const void* cpu_pointer=nullptr;
    const void* native_pointer=nullptr;
    std::uint64_t region=0;
    std::uint32_t family=0;
    bool double_sided=false,uniform_double_sided=true;
};
ALMetalWorldSourceCohort::ALMetalWorldSourceCohort(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceCohort::~ALMetalWorldSourceCohort()=default;
std::size_t ALMetalWorldSourceCohort::size() const noexcept{return mImpl->sources.size();}
std::uint32_t ALMetalWorldSourceCohort::family() const noexcept{return mImpl->family;}
bool ALMetalWorldSourceCohort::doubleSided() const noexcept{return mImpl->double_sided;}
bool ALMetalWorldSourceCohort::uniformDoubleSided() const noexcept{return mImpl->uniform_double_sided;}
std::size_t ALMetalWorldSourceCohort::metadataBytes() const noexcept
{return sizeof(*this)+sizeof(Impl)+mImpl->sources.capacity()*sizeof(Impl::Source);}
std::size_t ALMetalWorldSourceCohort::ownedBytes() const noexcept{return metadataBytes();}
std::uint64_t ALMetalWorldSourceCohort::regionIdentity() const noexcept{return mImpl->region;}
const ALMetalWorldSimpleBatch& ALMetalWorldSourceCohort::source(std::size_t index) const noexcept
{return mImpl->sources[index].prototype;}
const ALMetalWorldSourceKey& ALMetalWorldSourceCohort::key(std::size_t index) const noexcept
{return mImpl->sources[index].key;}
bool ALMetalWorldSourceCohort::matchesOwners(const std::shared_ptr<const void>& cpu,
                                           const std::shared_ptr<const void>& native) const noexcept
{return exactOwner(mImpl->cpu,mImpl->cpu_pointer,cpu) && exactOwner(mImpl->native,mImpl->native_pointer,native);}
bool ALMetalWorldSourceCohort::validView(const ALMetalWorldViewMatrices& view) noexcept
{
    // The texture matrix belongs to each sealed source, not the shared view.
    return finite(view.modelview_projection,16) && finite(view.modelview,16) && view.modelview[15]==1.f &&
        finite(&view.normal_matrix[0][0],12) && finite(&view.rigged_normal_matrix[0][0],12);
}
std::shared_ptr<const ALMetalWorldSourceCohort> ALMetalWorldSourceCohort::seal(
    std::span<const ALMetalWorldKeyedSource> sources,std::shared_ptr<const void> cpu,
    std::shared_ptr<const void> native,std::uint64_t region,std::string* error)
try
{
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldSourceCohort>
    {if(error)*error=message;return {};};
    if(sources.empty() || !cpu || !native || !region)
        return reject("Ordinary material cohort requires completed immutable producer ownership");
    auto impl=std::make_unique<Impl>();
    impl->sources.reserve(sources.size());
    ALMetalWorldViewMatrices canonical;
    for(unsigned axis=0;axis<4;++axis)
        canonical.modelview_projection[axis*5]=canonical.modelview[axis*5]=1.f;
    for(unsigned axis=0;axis<3;++axis)
        canonical.normal_matrix[axis][axis]=canonical.rigged_normal_matrix[axis][axis]=1.f;
    ALMetalWorldGBufferScene validation;validation.width=validation.height=1;
    for(const auto& keyed:sources)
    {
        if(!keyed.prototype || !keyed.key.identity || !keyed.key.revision)
            return reject("Ordinary material cohort source identity is incomplete");
        const auto& batch=*keyed.prototype;
        const std::uint32_t family=batch.material_slot_count?1u:(batch.pbr_slot_count?2u:0u);
        if(!family || (keyed.key.flavor&3u)!=family ||
           (impl->family && impl->family!=family) || batch.normal_texture || batch.skin_palette ||
           batch.skin_palette_float_count || batch.skin_joint_count || batch.weight_offset || batch.cloth_offset ||
           batch.avatar_rigid || batch.avatar_skinning || batch.avatar_cloth || batch.terrain || batch.impostor ||
           batch.impostor_fullbright || batch.impostor_shiny || batch.impostor_emboss ||
           !batch.native_material_source || !batch.native_material_source->matches(batch) ||
           !batch.native_resource_source || !batch.native_resource_source->matches(batch))
            return reject("Ordinary material cohort source is unsupported or unsealed");
        const ALMetalWorldDraw draw(batch,&canonical,batch.texture_matrix);
        // Preserve both ordinary normal-target ABIs. View matrices are
        // deliberately canonical here: the prototype's old camera is unused.
        for(bool normal_hdr:{false,true})
        {
            validation.normal_hdr=normal_hdr;
            if(!alMetalValidateWorldBatch(validation,draw,error))return {};
        }
        if(!impl->family){impl->family=family;impl->double_sided=batch.double_sided;}
        else impl->uniform_double_sided=impl->uniform_double_sided && impl->double_sided==batch.double_sided;
        impl->sources.push_back({keyed.key,batch});
    }
    // As with NativeScope/SourceReference, the completed producer is
    // responsible for the typed native union. A void owner cannot be
    // introspected to establish its contents. The actual group producer
    // verifies that union before issuing this immutable certificate.
    impl->cpu=cpu;impl->native=native;
    impl->cpu_pointer=cpu.get();impl->native_pointer=native.get();impl->region=region;
    return std::shared_ptr<const ALMetalWorldSourceCohort>(new ALMetalWorldSourceCohort(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}
