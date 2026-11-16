/**
 * @file almetalworldresources.mm
 * @brief Completion-owned native G-buffer resource tables.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldresources.h"
#include "almetalworld.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <new>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace
{
void fail(std::string* error,const char* text) { if(error)*error=text; }
void reason(ALMetalWorldResourceFallback* output,ALMetalWorldResourceFallback value) noexcept {if(output)*output=value;}
struct Key
{
    bool pbr=false;
    std::array<void*,32> views{};
    void* sampler=nullptr;
    bool operator==(const Key&) const=default;
};
struct KeyHash
{
    std::size_t operator()(const Key& key) const noexcept
    {
        std::size_t h=key.pbr;
        const auto mix=[&](void* value){h^=std::hash<void*>{}(value)+0x9e3779b9u+(h<<6)+(h>>2);};
        mix(key.sampler);for(auto value:key.views)mix(value);return h;
    }
};
struct Budget { std::atomic<std::size_t> tables{0},bytes{0}; };
struct State
{
    const ALMetalDevice* wrapper=nullptr;
    id<MTLDevice> device=nil;
    id<MTLLibrary> library=nil;
    std::array<id<MTLArgumentEncoder>,2> encoders{};
    ALMetalWorldResourceLimits limits;
    ALMetalWorldResourceStats stats;
    std::shared_ptr<Budget> budget=std::make_shared<Budget>();
    std::unordered_map<Key,std::weak_ptr<const ALMetalWorldResourceTable>,KeyHash> lookup;
    std::uint64_t epoch=1;
    bool supported=false,force_failure=false;
};
bool sameContext(const std::weak_ptr<State>& weak,
                 const std::shared_ptr<State>& owner) noexcept
{
    // Control-block identity needs no temporary strong owner. The caller
    // already keeps this exact context alive for the entire operation.
    return !weak.owner_before(owner) && !owner.owner_before(weak);
}
void rejected(State& state,ALMetalWorldResourceFallback value,ALMetalWorldResourceFallback* output) noexcept
{
    ++state.stats.failures;state.stats.last_failure=value;reason(output,value);
}
struct CommandOwner
{
    struct Held {std::shared_ptr<const ALMetalWorldResourceSource> source;std::shared_ptr<const ALMetalWorldResourceTable> table;};
    std::vector<Held> held;
    struct Identity {const ALMetalWorldResourceSource* source=nullptr;const ALMetalWorldResourceTable* table=nullptr;};
    std::array<Identity,16> seen{};

    void retain(const std::shared_ptr<const ALMetalWorldResourceSource>& source,
                const std::shared_ptr<const ALMetalWorldResourceTable>& table)
    {
        const auto source_bits=reinterpret_cast<std::uintptr_t>(source.get());
        const auto table_bits=reinterpret_cast<std::uintptr_t>(table.get());
        const auto slot=((source_bits>>4)^(source_bits>>12)^
                         (table_bits>>6)^(table_bits>>14))&(seen.size()-1);
        auto& identity=seen[slot];
        if(identity.source==source.get() && identity.table==table.get()) return;
        held.push_back({source,table});
        // Mark only after ownership succeeds. Collisions cause another retain,
        // never a false hit; held prevents either raw identity being recycled.
        identity={source.get(),table.get()};
    }
};
}

struct ALMetalWorldResourceSource::Impl
{
    void* device=nullptr;
    std::shared_ptr<const ALMetalWorldMaterialSource> material;
    std::uint32_t count=0;
    std::array<const ALMetalTexture*,8> diffuse{};
    Key key;
    std::array<id<MTLTexture>,32> views{};
    std::vector<std::shared_ptr<const ALMetalTexture>> leases;
    // Small sampler/library working set, never an owner of a context.
    mutable std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> tables;
};
struct ALMetalWorldResourceTable::Impl
{
    Key key;
    std::weak_ptr<State> context;
    std::uint64_t epoch=0;
    std::shared_ptr<const ALMetalBuffer> buffer;
    std::array<id<MTLTexture>,32> textures{};
    id<MTLSamplerState> sampler=nil;
    std::vector<void*> resources;
    std::weak_ptr<const void> declared_encoder;
    std::shared_ptr<Budget> budget;
    std::size_t charged=0;
    ~Impl()
    {
        if(budget){budget->tables.fetch_sub(1,std::memory_order_relaxed);budget->bytes.fetch_sub(charged,std::memory_order_relaxed);}
    }
};
struct ALMetalWorldResourceDeclaration::Impl
{
    std::weak_ptr<State> context;
    std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> tables;
    std::vector<void*> resources;
};
ALMetalWorldResourceDeclaration::ALMetalWorldResourceDeclaration(std::unique_ptr<Impl> impl)
    :mImpl(std::move(impl)){}
ALMetalWorldResourceDeclaration::~ALMetalWorldResourceDeclaration()=default;
std::size_t ALMetalWorldResourceDeclaration::metadataBytes()const noexcept
{return sizeof(*this)+sizeof(Impl)+mImpl->tables.capacity()*sizeof(mImpl->tables[0])+
    mImpl->resources.capacity()*sizeof(mImpl->resources[0]);}
std::size_t ALMetalWorldResourceDeclaration::resourceCount()const noexcept{return mImpl->resources.size();}
struct ALMetalWorldResources::Impl
{
    std::shared_ptr<State> state;
    std::weak_ptr<const ALMetalCommandCompletion> owner_command;
    std::weak_ptr<CommandOwner> owner;
};
struct ALMetalWorldResourcePreparation::Impl
{
    std::shared_ptr<State> state;
    std::uint64_t epoch=0;
    std::shared_ptr<const ALMetalWorldResourceSource> source;
    id<MTLSamplerState> sampler=nil;
    Key key;
    std::shared_ptr<const ALMetalWorldResourceTable> existing;
    std::size_t bytes=0;
};
ALMetalWorldResourcePreparation::ALMetalWorldResourcePreparation(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldResourcePreparation::~ALMetalWorldResourcePreparation()=default;
std::shared_ptr<const ALMetalWorldResourceTable> ALMetalWorldResourcePreparation::existingTable()const noexcept{return mImpl->existing;}
std::size_t ALMetalWorldResourcePreparation::publicationBytes()const noexcept{return mImpl->bytes;}
std::size_t ALMetalWorldResourcePreparation::publicationHash()const noexcept
{return KeyHash{}(mImpl->key)^std::hash<const void*>{}(mImpl->state.get())^std::hash<std::uint64_t>{}(mImpl->epoch);}
bool ALMetalWorldResourcePreparation::samePublication(const ALMetalWorldResourcePreparation& other)const noexcept
{return mImpl->state==other.mImpl->state&&mImpl->epoch==other.mImpl->epoch&&
    ((mImpl->existing&&mImpl->existing==other.mImpl->existing)||mImpl->key==other.mImpl->key);}
ALMetalWorldResourceSource::ALMetalWorldResourceSource(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldResourceSource::~ALMetalWorldResourceSource()=default;
ALMetalWorldResourceTable::ALMetalWorldResourceTable(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldResourceTable::~ALMetalWorldResourceTable()=default;
std::shared_ptr<const ALMetalBuffer> ALMetalWorldResourceTable::argumentLease()const noexcept{return mImpl->buffer;}
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
    impl->count=batch.texture_count;impl->key.pbr=batch.pbr_slot_count!=0;
    const auto& values=static_cast<const ALMetalWorldSimpleBatch&>(batch);
    const bool normal=batch.material_slot_count &&values.materialSlots()[0].normal_texture;
    const bool specular=batch.material_slot_count &&values.materialSlots()[0].specular_texture;
    const auto snapshot=[&](std::size_t index,const ALMetalTexture* texture,bool linear)
    {
        const auto found=std::find_if(leases.begin(),leases.end(),[&](const auto& lease){return lease.get()==texture;});
        if(!texture ||found==leases.end()){reason(failure,ALMetalWorldResourceFallback::validation);return false;}
        if(std::none_of(impl->leases.begin(),impl->leases.end(),[&](const auto& lease){return lease.get()==texture;}))impl->leases.push_back(*found);
        id<MTLTexture> native=(__bridge id<MTLTexture>)(linear?texture->nativeLinearHandle():texture->nativeHandle());
        if(!native){reason(failure,ALMetalWorldResourceFallback::native_allocation);return false;}
        if(native.device!=(__bridge id<MTLDevice>)device.nativeHandle() ||native.textureType!=MTLTextureType2D ||native.sampleCount!=1)
        {reason(failure,ALMetalWorldResourceFallback::validation);return false;}
        impl->views[index]=native;impl->key.views[index]=(__bridge void*)native;return true;
    };
    for(std::size_t slot=0;slot<8;++slot)
    {
        const auto at=std::min<std::size_t>(slot,batch.texture_count-1);
        if(slot<batch.texture_count)impl->diffuse[slot]=batch.diffuse_textures[slot];
        if(!snapshot(slot,batch.diffuse_textures[at],false)){fail(error,"Native world diffuse generation is unowned or invalid");return {};}
        if(impl->key.pbr)
        {
            const auto& material=values.pbrSlots()[at];
            if(!snapshot(8+slot,material.normal_texture,true)||!snapshot(16+slot,material.orm_texture,true)||!snapshot(24+slot,material.emissive_texture,false))
            {fail(error,"Native world PBR generation is unowned or invalid");return {};}
        }
        else
        {
            const auto& material=values.materialSlots()[at];
            if(!snapshot(8+slot,normal?material.normal_texture:batch.diffuse_textures[0],true)||
               !snapshot(16+slot,specular?material.specular_texture:batch.diffuse_textures[0],false))
            {fail(error,"Native world legacy generation is unowned or invalid");return {};}
        }
    }
    auto result=std::shared_ptr<const ALMetalWorldResourceSource>(new ALMetalWorldResourceSource(std::move(impl)));
    batch.native_resource_source=result;return result;
}
catch(const std::bad_alloc&){reason(failure,ALMetalWorldResourceFallback::host_allocation);if(error)error->clear();return {};}

ALMetalWorldResources::ALMetalWorldResources(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldResources::~ALMetalWorldResources()=default;
std::shared_ptr<ALMetalWorldResources> ALMetalWorldResources::create(
    const ALMetalDevice& device,const ALMetalShaderLibrary& library,ALMetalWorldResourceLimits limits,std::string* error,ALMetalWorldResourceFallback* failure)
try
{
    reason(failure,ALMetalWorldResourceFallback::none);
    auto impl=std::make_unique<Impl>();impl->state=std::make_shared<State>();auto& state=*impl->state;
    state.wrapper=&device;state.device=(__bridge id<MTLDevice>)device.nativeHandle();state.library=(__bridge id<MTLLibrary>)library.nativeHandle();state.limits=limits;
    if(!state.device ||state.library.device!=state.device){reason(failure,ALMetalWorldResourceFallback::validation);fail(error,"Native world resource library/device mismatch");return {};}
    state.supported=state.device.argumentBuffersSupport==MTLArgumentBuffersTier2;
    if(state.supported)
    {
        constexpr const char* names[]{"almetal_world_material_resources_fragment","almetal_world_pbr_resources_fragment"};
        for(unsigned family=0;family<2;++family)
        {
            auto function=[state.library newFunctionWithName:[NSString stringWithUTF8String:names[family]]];
            state.encoders[family]=[function newArgumentEncoderWithBufferIndex:1];
            ++state.stats.argument_encoder_creations;
            if(!state.encoders[family] ||!state.encoders[family].encodedLength){reason(failure,ALMetalWorldResourceFallback::shader_abi);fail(error,"Native world resource shader ABI is unavailable");return {};}
        }
    }
    return std::shared_ptr<ALMetalWorldResources>(new ALMetalWorldResources(std::move(impl)));
}
catch(const std::bad_alloc&){reason(failure,ALMetalWorldResourceFallback::host_allocation);if(error)error->clear();return {};}
bool ALMetalWorldResources::supported()const noexcept{return mImpl->state->supported;}
std::shared_ptr<const ALMetalWorldResourceTable> ALMetalWorldResources::cachedSourceTable(
    const std::shared_ptr<const ALMetalWorldResourceSource>& source,const ALMetalSamplerState& sampler)const noexcept
{
    const auto& state=mImpl->state;id<MTLSamplerState> native=(__bridge id<MTLSamplerState>)sampler.nativeHandle();
    if(state->supported&&source&&sampler.supportsArgumentBuffers()&&native)
        for(const auto& table:source->mImpl->tables)
            if(sameContext(table->mImpl->context,state)&&table->mImpl->epoch==state->epoch&&table->mImpl->key.sampler==(__bridge void*)native)return table;
    return {};
}
std::shared_ptr<const ALMetalWorldResourcePreparation> ALMetalWorldResources::preflight(
    const std::shared_ptr<const ALMetalWorldResourceSource>& source,const ALMetalSamplerState& sampler,
    std::string* error,ALMetalWorldResourceFallback* failure)const
try
{
    reason(failure,ALMetalWorldResourceFallback::none);
    const auto& state=mImpl->state;
    id<MTLSamplerState> native=(__bridge id<MTLSamplerState>)sampler.nativeHandle();
    auto cached=cachedSourceTable(source,sampler);
    if(!cached&&(!state->supported||!source||source->mImpl->device!=(__bridge void*)state->device||
       !sampler.supportsArgumentBuffers()||!native||native.device!=state->device)
       )
    {reason(failure,state->supported?ALMetalWorldResourceFallback::validation:ALMetalWorldResourceFallback::unsupported);fail(error,"Native world resource preparation is unsupported or invalid");return {};}
    auto plan=std::make_unique<ALMetalWorldResourcePreparation::Impl>();
    plan->state=state;plan->epoch=state->epoch;plan->source=source;plan->sampler=native;
    if(cached){plan->existing=cached;plan->key=cached->mImpl->key;}
    if(!plan->existing)
    {
        plan->key=source->mImpl->key;plan->key.sampler=(__bridge void*)native;
        if(auto found=state->lookup.find(plan->key);found!=state->lookup.end())plan->existing=found->second.lock();
    }
    if(!plan->existing)plan->bytes=state->encoders[plan->key.pbr?1:0].encodedLength;
    return std::shared_ptr<const ALMetalWorldResourcePreparation>(new ALMetalWorldResourcePreparation(std::move(plan)));
}
catch(const std::bad_alloc&){reason(failure,ALMetalWorldResourceFallback::host_allocation);if(error)error->clear();return {};}
bool ALMetalWorldResources::validatePreparations(
    std::span<const std::shared_ptr<const ALMetalWorldResourcePreparation>> plans,std::string* error)const
{
    const auto& state=mImpl->state;std::size_t count=0,bytes=0;
    for(const auto& plan:plans)
    {
        if(!plan||plan->mImpl->state!=state||plan->mImpl->epoch!=state->epoch)
        {fail(error,"Native resource preparation proof is stale");return false;}
        if(plan->mImpl->bytes)
        {
            if(count==std::numeric_limits<std::size_t>::max()||plan->mImpl->bytes>std::numeric_limits<std::size_t>::max()-bytes)
            {fail(error,"Native resource preparation size overflows");return false;}
            ++count;bytes+=plan->mImpl->bytes;
        }
    }
    const auto live=state->budget->tables.load(std::memory_order_relaxed),used=state->budget->bytes.load(std::memory_order_relaxed);
    if(live>state->limits.maximum_tables||count>state->limits.maximum_tables-live||
       used>state->limits.maximum_buffer_bytes||bytes>state->limits.maximum_buffer_bytes-used)
    {fail(error,"Native world resource table capacity is exhausted");return false;}
    return true;
}
std::shared_ptr<const ALMetalWorldResourceTable> ALMetalWorldResources::prepare(
    const std::shared_ptr<const ALMetalWorldResourceSource>& source,const ALMetalSamplerState& sampler,std::string* error,ALMetalWorldResourceFallback* failure)
{
    const auto& state=mImpl->state;
    if(state->force_failure)
    {state->force_failure=false;++state->stats.requests;rejected(*state,ALMetalWorldResourceFallback::injected,failure);fail(error,"Forced optional native resource preparation failure");return {};}
    // Preserve the existing allocation-free ordinary bind fast path. Both
    // direct preparation and read-only planning share its exact proof.
    if(auto table=cachedSourceTable(source,sampler))
    {++state->stats.requests;++state->stats.cache_hits;reason(failure,ALMetalWorldResourceFallback::none);return table;}
    ALMetalWorldResourceFallback why=ALMetalWorldResourceFallback::none;
    auto plan=preflight(source,sampler,error,&why);
    if(plan)return prepare(*plan,error,failure);
    ++mImpl->state->stats.requests;rejected(*mImpl->state,why,failure);return {};
}
std::shared_ptr<const ALMetalWorldResourceTable> ALMetalWorldResources::prepare(
    const ALMetalWorldResourcePreparation& preparation,std::string* error,ALMetalWorldResourceFallback* failure)
try
{
    reason(failure,ALMetalWorldResourceFallback::none);
    const auto& state=mImpl->state;++state->stats.requests;
    if(state->force_failure){state->force_failure=false;rejected(*state,ALMetalWorldResourceFallback::injected,failure);fail(error,"Forced optional native resource preparation failure");return {};}
    const auto& plan=*preparation.mImpl;
    if(plan.state!=state||plan.epoch!=state->epoch)
    {rejected(*state,ALMetalWorldResourceFallback::validation,failure);fail(error,"Native resource preparation proof is stale");return {};}
    const auto& source=plan.source;const auto native=plan.sampler;const auto& key=plan.key;
    auto& cached=source->mImpl->tables;
    std::erase_if(cached,[&](const auto& table){return table->mImpl->context.expired()||
        (sameContext(table->mImpl->context,state)&&(table->mImpl->epoch!=state->epoch||table->mImpl->key.sampler!=key.sampler));});
    auto existing=plan.existing;
    if(!existing)if(auto at=state->lookup.find(key);at!=state->lookup.end())existing=at->second.lock();
    if(existing)
    {
        ++state->stats.cache_hits;
        if(std::none_of(cached.begin(),cached.end(),[&](const auto& table){return table==existing;}))
        {if(cached.size()==4)cached.erase(cached.begin());cached.push_back(existing);}
        return existing;
    }
    auto encoder=state->encoders[key.pbr?1:0];const std::size_t length=encoder.encodedLength;
    const auto live=state->budget->tables.load(std::memory_order_relaxed),bytes=state->budget->bytes.load(std::memory_order_relaxed);
    if(live>=state->limits.maximum_tables ||length>state->limits.maximum_buffer_bytes ||bytes>state->limits.maximum_buffer_bytes-length)
    {rejected(*state,ALMetalWorldResourceFallback::capacity,failure);fail(error,"Native world resource table capacity is exhausted");return {};}
    ALMetalBufferDescriptor descriptor;descriptor.length=length;descriptor.storage=ALMetalStorageMode::shared;descriptor.label="Alchemy immutable world resource arguments";
    std::shared_ptr<ALMetalBuffer> buffer=ALMetalBuffer::create(*state->wrapper,descriptor,error);
    if(!buffer){rejected(*state,ALMetalWorldResourceFallback::native_allocation,failure);return {};}
    auto value=std::make_unique<ALMetalWorldResourceTable::Impl>();value->key=key;value->context=state;value->epoch=state->epoch;
    value->buffer=buffer;value->textures=source->mImpl->views;value->sampler=native;
    const std::size_t count=key.pbr?32:24;
    value->resources.reserve(count);
    for(std::size_t slot=0;slot<count;++slot)
    {
        auto handle=key.views[slot];if(std::find(value->resources.begin(),value->resources.end(),handle)==value->resources.end())value->resources.push_back(handle);
    }
    // Complete all throwing CPU container work before the encoder can retain
    // this buffer. No C++ allocation occurs between binding and detaching it.
    [encoder setArgumentBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle() offset:0];
    for(std::size_t slot=0;slot<count;++slot)[encoder setTexture:value->textures[slot] atIndex:slot];
    [encoder setSamplerState:native atIndex:count];
    [encoder setArgumentBuffer:nil offset:0];
    value->charged=length;value->budget=state->budget;
    value->budget->tables.fetch_add(1,std::memory_order_relaxed);value->budget->bytes.fetch_add(length,std::memory_order_relaxed);
    auto table=std::shared_ptr<const ALMetalWorldResourceTable>(new ALMetalWorldResourceTable(std::move(value)));
    if(state->lookup.size()>=state->limits.maximum_tables)std::erase_if(state->lookup,[](const auto& entry){return entry.second.expired();});
    state->lookup[key]=table;if(cached.size()==4)cached.erase(cached.begin());cached.push_back(table);
    ++state->stats.table_creates;state->stats.published_bytes+=length;return table;
}
catch(const std::bad_alloc&){rejected(*mImpl->state,ALMetalWorldResourceFallback::host_allocation,failure);if(error)error->clear();return {};}

bool ALMetalWorldResources::bind(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const std::shared_ptr<const ALMetalWorldResourceSource>& source,const std::shared_ptr<const ALMetalWorldResourceTable>& table,std::string* error,ALMetalWorldResourceFallback* failure)
try
{
    reason(failure,ALMetalWorldResourceFallback::none);
    const auto& state=mImpl->state;
    // Source-held membership certifies the immutable tuple. An older valid
    // table can outlive lookup invalidation/eviction, so preserve the complete
    // tuple check for such direct callers instead of rejecting their lease.
    const bool source_table=source && table && std::any_of(source->mImpl->tables.begin(),source->mImpl->tables.end(),
        [&](const auto& cached){return cached.get()==table.get();});
    if(!source||!table||!sameContext(table->mImpl->context,state) ||
        (!source_table&&(table->mImpl->key.views!=source->mImpl->key.views ||table->mImpl->key.pbr!=source->mImpl->key.pbr)) ||
        encoder.nativeDeviceForIndirect()!=(__bridge void*)state->device||encoder.nativeCommandBufferForIndirect()!=command.nativeHandle()||command.isCommitted())
    {rejected(*state,ALMetalWorldResourceFallback::validation,failure);return false;}
    auto completion=command.completionToken();if(!completion){rejected(*state,ALMetalWorldResourceFallback::retention,failure);return false;}
    auto owner=mImpl->owner.lock();
    if(!owner||mImpl->owner_command.lock()!=completion)
    {
        owner=std::make_shared<CommandOwner>();if(!command.retainUntilCompletion(owner,error)){rejected(*state,ALMetalWorldResourceFallback::retention,failure);return false;}
        mImpl->owner=owner;mImpl->owner_command=completion;
    }
    owner->retain(source,table);
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForResources();
    if(!native){rejected(*state,ALMetalWorldResourceFallback::declaration,failure);return false;}
    const auto identity=encoder.declarationIdentity(true);
    if(!identity){rejected(*state,ALMetalWorldResourceFallback::declaration,failure);return false;}
    if(table->mImpl->declared_encoder.owner_before(identity)||identity.owner_before(table->mImpl->declared_encoder))
    {
        const auto* resources=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(table->mImpl->resources.data());
        [native useResources:resources count:table->mImpl->resources.size() usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        table->mImpl->declared_encoder=identity;state->stats.declared_resources+=table->mImpl->resources.size();
    }
    if(!encoder.setFragmentBuffer(1,*table->mImpl->buffer,0,error)){rejected(*state,ALMetalWorldResourceFallback::declaration,failure);return false;}
    ++state->stats.buffer_bind_calls;return true;
}
catch(const std::bad_alloc&){rejected(*mImpl->state,ALMetalWorldResourceFallback::host_allocation,failure);if(error)error->clear();return false;}
bool ALMetalWorldResources::declareResources(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const std::shared_ptr<const ALMetalWorldResourceTable>& table,std::string* error,ALMetalWorldResourceFallback* failure)
try
{
    reason(failure,ALMetalWorldResourceFallback::none);
    const auto& state=mImpl->state;
    if(!table || !sameContext(table->mImpl->context,state) ||
       encoder.nativeDeviceForIndirect()!=(__bridge void*)state->device ||
       encoder.nativeCommandBufferForIndirect()!=command.nativeHandle() || command.isCommitted())
    {rejected(*state,ALMetalWorldResourceFallback::validation,failure);return false;}
    if(!command.retainUntilCompletion(table,error))
    {rejected(*state,ALMetalWorldResourceFallback::retention,failure);return false;}
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForResources();
    if(!native){rejected(*state,ALMetalWorldResourceFallback::declaration,failure);return false;}
    const auto identity=encoder.declarationIdentity(true);
    if(!identity){rejected(*state,ALMetalWorldResourceFallback::declaration,failure);return false;}
    if(table->mImpl->declared_encoder.owner_before(identity)||identity.owner_before(table->mImpl->declared_encoder))
    {
        const auto* resources=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(table->mImpl->resources.data());
        [native useResources:resources count:table->mImpl->resources.size() usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        table->mImpl->declared_encoder=identity;state->stats.declared_resources+=table->mImpl->resources.size();
    }
    // The source-row shader reaches this buffer through another argument
    // buffer; ordinary bind() directly binds it and does not need this call.
    [native useResource:(__bridge id<MTLBuffer>)table->mImpl->buffer->nativeHandle()
        usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
    return true;
}
catch(const std::bad_alloc&){rejected(*mImpl->state,ALMetalWorldResourceFallback::host_allocation,failure);if(error)error->clear();return false;}
std::shared_ptr<const ALMetalWorldResourceDeclaration> ALMetalWorldResources::prepareDeclaration(
    std::span<const std::shared_ptr<const ALMetalWorldResourceTable>> tables,
    std::size_t maximum_metadata_bytes,std::string* error)
try
{
    const auto& state=mImpl->state;
    auto impl=std::make_unique<ALMetalWorldResourceDeclaration::Impl>();impl->context=state;
    if(tables.empty()||tables.size()>state->limits.maximum_tables)
    {fail(error,"Cohort declaration table count is invalid");return {};}
    std::unordered_set<const ALMetalWorldResourceTable*> unique_tables;
    std::unordered_set<void*> unique_resources;
    for(const auto& table:tables)
    {
        if(!table||!sameContext(table->mImpl->context,state))
        {fail(error,"Cohort declaration requires exact resource context tables");return {};}
        if(!unique_tables.insert(table.get()).second)continue;
        impl->tables.push_back(table);
        const auto append=[&](void* resource)
        {if(resource&&unique_resources.insert(resource).second)impl->resources.push_back(resource);};
        append(table->mImpl->buffer->nativeHandle());
        for(auto resource:table->mImpl->resources)append(resource);
        // Check actual retained capacity, not only the number of unique handles.
        const auto bytes=sizeof(ALMetalWorldResourceDeclaration)+sizeof(ALMetalWorldResourceDeclaration::Impl)+
            impl->tables.capacity()*sizeof(impl->tables[0])+impl->resources.capacity()*sizeof(impl->resources[0]);
        if(bytes>maximum_metadata_bytes)
        {fail(error,"Cohort declaration metadata capacity is exhausted");return {};}
    }
    return std::shared_ptr<const ALMetalWorldResourceDeclaration>(new ALMetalWorldResourceDeclaration(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

bool ALMetalWorldResources::declareCertifiedResources(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const ALMetalWorldResourceDeclaration& declaration,std::string* error)
{
    const auto& state=mImpl->state;
    if(!sameContext(declaration.mImpl->context,state)||command.isCommitted()||
       encoder.nativeDeviceForIndirect()!=(__bridge void*)state->device||
       encoder.nativeCommandBufferForIndirect()!=command.nativeHandle())
    {fail(error,"Cohort declaration requires its exact context and active command");return false;}
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForResources();
    if(!native){fail(error,"Cohort declaration requires a live render encoder");return false;}
    const auto* resources=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(declaration.mImpl->resources.data());
    [native useResources:resources count:declaration.mImpl->resources.size()
        usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
    state->stats.declared_resources+=declaration.mImpl->resources.size();return true;
}
void ALMetalWorldResources::releaseSourceCache(const std::shared_ptr<const ALMetalWorldResourceSource>& source)noexcept
{
    if(source)std::erase_if(source->mImpl->tables,[&](const auto& table)
        {return sameContext(table->mImpl->context,mImpl->state);});
}
void ALMetalWorldResources::clearLookup()noexcept{mImpl->state->lookup.clear();++mImpl->state->epoch;}
void ALMetalWorldResources::failNextPreparationForTesting()noexcept{mImpl->state->force_failure=true;}
ALMetalWorldResourceStats ALMetalWorldResources::stats()const noexcept
{
    auto result=mImpl->state->stats;result.lookup_entries=mImpl->state->lookup.size();
    result.live_tables=mImpl->state->budget->tables.load(std::memory_order_relaxed);result.live_buffer_bytes=mImpl->state->budget->bytes.load(std::memory_order_relaxed);return result;
}
