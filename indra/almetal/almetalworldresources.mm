/**
 * @file almetalworldresources.mm
 * @brief Immutable native texture-generation ownership for CPU world sources.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldresources.h"
#include "almetalworld.h"
#include "almetalresourceplan.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <new>
#include <vector>

namespace
{
namespace RP = ALMetalResourcePlanning;
void appendPlanView(std::vector<RP::View>& views, const RP::View& view)
{
    if (view && std::none_of(views.begin(), views.end(), [&](const auto& known)
        { return known.identity() == view.identity() && known.generation() == view.generation(); }))
        views.push_back(view);
}
void fail(std::string* error, const char* text) { if (error) *error = text; }
void reason(ALMetalWorldResourceFallback* output, ALMetalWorldResourceFallback value) noexcept
{ if (output) *output = value; }
}

struct ALMetalWorldResourceSource::Impl
{
    void* device = nullptr;
    std::shared_ptr<const ALMetalWorldMaterialSource> material;
    std::uint32_t count = 0;
    std::array<const ALMetalTexture*, 8> diffuse{};
    std::vector<std::shared_ptr<const ALMetalTexture>> leases;
    // These aliases also protect a CPU-published generation before a command
    // exists. Native render-target reuse must not overwrite that generation.
    std::vector<RP::View> plan_views;
};
ALMetalWorldResourceSource::ALMetalWorldResourceSource(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldResourceSource::~ALMetalWorldResourceSource()=default;
std::span<const std::shared_ptr<const ALMetalTexture>> ALMetalWorldResourceSource::generationLeases()const noexcept
{return mImpl->leases;}
bool ALMetalWorldResourceSource::matches(const ALMetalWorldSimpleBatch& batch)const noexcept
{
    if(batch.native_material_source!=mImpl->material || !mImpl->material->matches(batch) ||batch.texture_count!=mImpl->count)return false;
    for(std::size_t i=0;i<mImpl->count;++i)if(batch.diffuse_textures[i]!=mImpl->diffuse[i])return false;
    return true;
}
std::shared_ptr<const ALMetalWorldResourceSource> ALMetalWorldResourceSource::publish(
    const ALMetalDevice& device,ALMetalWorldSimpleBatch& batch,
    std::span<const std::shared_ptr<const ALMetalTexture>> leases,std::string* error,ALMetalWorldResourceFallback* failure)
try
{
    reason(failure,ALMetalWorldResourceFallback::none);
    if(batch.native_resource_source &&batch.native_resource_source->matches(batch) &&batch.native_resource_source->mImpl->device==device.nativeHandle())return batch.native_resource_source;
    if(!device.nativeHandle() ||!batch.native_material_source ||!batch.native_material_source->matches(batch) ||!batch.texture_count ||batch.texture_count>8)
    {reason(failure,!batch.native_material_source?ALMetalWorldResourceFallback::missing_source:
        !batch.native_material_source->matches(batch)?ALMetalWorldResourceFallback::unmatched_source:ALMetalWorldResourceFallback::validation);
        fail(error,"Native world resource source needs a matched immutable material");return {};}
    auto impl=std::make_unique<Impl>();impl->device=device.nativeHandle();impl->material=batch.native_material_source;
    impl->count=batch.texture_count;
    const bool pbr=batch.pbr_slot_count!=0;
    const auto& values=static_cast<const ALMetalWorldSimpleBatch&>(batch);
    const bool normal=batch.material_slot_count &&values.materialSlots()[0].normal_texture;
    const bool specular=batch.material_slot_count &&values.materialSlots()[0].specular_texture;
    const auto snapshot=[&](const ALMetalTexture* texture,bool linear)
    {
        const auto found=std::find_if(leases.begin(),leases.end(),[&](const auto& lease){return lease.get()==texture;});
        if(!texture ||found==leases.end()){reason(failure,ALMetalWorldResourceFallback::validation);return false;}
        if(std::none_of(impl->leases.begin(),impl->leases.end(),[&](const auto& lease){return lease.get()==texture;}))impl->leases.push_back(*found);
        id<MTLTexture> native=(__bridge id<MTLTexture>)(linear?texture->nativeLinearHandle():texture->nativeHandle());
        if(!native){reason(failure,ALMetalWorldResourceFallback::native_allocation);return false;}
        if(native.device!=(__bridge id<MTLDevice>)device.nativeHandle() ||native.textureType!=MTLTextureType2D ||native.sampleCount!=1)
        {reason(failure,ALMetalWorldResourceFallback::validation);return false;}
        const auto& plan=texture->resourceView();appendPlanView(impl->plan_views,plan);return true;
    };
    for(std::size_t slot=0;slot<8;++slot)
    {
        const auto at=std::min<std::size_t>(slot,batch.texture_count-1);
        if(slot<batch.texture_count)impl->diffuse[slot]=batch.diffuse_textures[slot];
        if(!snapshot(batch.diffuse_textures[at],false)){fail(error,"Native world diffuse generation is unowned or invalid");return {};}
        if(pbr)
        {
            const auto& material=values.pbrSlots()[at];
            if(!snapshot(material.normal_texture,true)||!snapshot(material.orm_texture,true)||!snapshot(material.emissive_texture,false))
            {fail(error,"Native world PBR generation is unowned or invalid");return {};}
        }
        else
        {
            const auto& material=values.materialSlots()[at];
            if(!snapshot(normal?material.normal_texture:batch.diffuse_textures[0],true)||
               !snapshot(specular?material.specular_texture:batch.diffuse_textures[0],false))
            {fail(error,"Native world legacy generation is unowned or invalid");return {};}
        }
    }
    auto result=std::shared_ptr<const ALMetalWorldResourceSource>(new ALMetalWorldResourceSource(std::move(impl)));
    batch.native_resource_source=result;return result;
}
catch(const std::bad_alloc&){reason(failure,ALMetalWorldResourceFallback::host_allocation);if(error)error->clear();return {};}
