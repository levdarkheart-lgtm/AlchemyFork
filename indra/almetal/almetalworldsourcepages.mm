/**
 * @file almetalworldsourcepages.mm
 * @brief Reflection-encoded source pages with explicit actual-view readers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldsourcepages.h"
#include "almetalworldanimated.h"
#include "almetalworldprepare.h"
#include "almetalworldcohort.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <iterator>
#include <list>
#include <new>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace
{
namespace RP=ALMetalResourcePlanning;
void appendPlanView(std::vector<RP::View>& views,const RP::View& view)
{
    if(view&&std::none_of(views.begin(),views.end(),[&](const auto& known)
        {return known.identity()==view.identity()&&known.generation()==view.generation();}))views.push_back(view);
}

void fail(std::string* error,const char* message){if(error)*error=message;}
struct Budget {std::atomic<std::size_t> bytes{0};std::size_t limit=0;};
struct DeclarationMetadata
{
    std::shared_ptr<Budget> budget;
    std::size_t bytes=0;
    ~DeclarationMetadata(){if(budget)budget->bytes.fetch_sub(bytes,std::memory_order_relaxed);}
};
struct DirectSlotAccounting {std::atomic<std::size_t> count{0},bytes{0};};
struct Buffer
{
    std::shared_ptr<const ALMetalBuffer> value;
    std::shared_ptr<Budget> budget;
    std::size_t charged_bytes=0;
    ~Buffer(){if(value&&budget)budget->bytes.fetch_sub(charged_bytes,std::memory_order_relaxed);}
};
struct Page;
struct PageReader {std::shared_ptr<const Page> page;};
struct Page
{
    std::shared_ptr<Buffer> buffer;
    std::weak_ptr<PageReader> reader;
};
struct Bank;
struct BankReader {std::shared_ptr<const Bank> bank;};
struct Bank
{
    std::shared_ptr<Buffer> buffer;
    std::weak_ptr<BankReader> reader;
};
struct Key
{
    std::uint64_t identity=0,flavor=0;
    bool operator==(const Key&)const=default;
};
struct KeyHash {std::size_t operator()(const Key& key)const noexcept
{return std::hash<std::uint64_t>{}(key.identity)^(std::hash<std::uint64_t>{}(key.flavor)<<1);}};
struct Signature
{
    std::array<void*,12> handles{};
    std::array<std::size_t,7> offsets{};
    std::array<std::uint32_t,24> values{};
    bool operator==(const Signature&)const=default;
};
struct CohortValidity {bool valid=true;};
// Intrusive reverse dependencies belong only to stable CPU cache entries.
// Rows and actual FrameViews own no watcher allocations or producer graph.
struct CohortRowWatch
{
    CohortRowWatch* next=nullptr;
    CohortRowWatch* previous=nullptr;
    CohortRowWatch** head=nullptr;
    CohortValidity* entry=nullptr;
    ~CohortRowWatch()
    {
        if(!head)return;
        if(previous)previous->next=next;else *head=next;
        if(next)next->previous=previous;
    }
};
struct Row
{
    Key key;
    std::uint64_t revision=0,minimum_revision=0,last_use=0;
    std::uint32_t generation=0;
    bool allocated=false,valid=false;
    CohortRowWatch* watchers=nullptr;
    Signature signature;
    std::list<std::uint32_t>::iterator lru;
    std::weak_ptr<const ALMetalWorldMaterialSource> material;
    std::weak_ptr<const ALMetalWorldResourceSource> resources;
    std::weak_ptr<const ALMetalWorldResourceTable> table;
};
struct Context
{
    const ALMetalDevice* device=nullptr;
    id<MTLDevice> native=nil;
    id<MTLLibrary> library=nil;
    id<MTLArgumentEncoder> bank_encoder=nil,row_encoder=nil;
    std::unique_ptr<ALMetalSamplerState> sampler;
    std::shared_ptr<ALMetalWorldResources> resources;
    std::shared_ptr<Budget> budget=std::make_shared<Budget>();
    std::size_t stride=0,bank_length=0;
    std::uint32_t anisotropy=1;
    std::uint64_t direct_asset_limit=0;
    std::atomic<std::uint64_t> binds{0};
    std::atomic<std::uint64_t> cohort_declaration_calls{0},cohort_declared_resources{0};
    std::shared_ptr<DirectSlotAccounting> direct_slots=std::make_shared<DirectSlotAccounting>();
};
std::shared_ptr<Buffer> makeBuffer(const std::shared_ptr<Context>& context,std::size_t length,
                                  const char* label,std::string* error)
{
    const auto live=context->budget->bytes.load(std::memory_order_relaxed);
    // The table cache has a separate reserved partition of the same total
    // metadata limit, so a later table allocation cannot exceed the cap.
    if(!length || live>context->budget->limit || length>context->budget->limit-live)
    {fail(error,"Native source metadata capacity is exhausted");return {};}
    auto result=std::make_shared<Buffer>();
    ALMetalBufferDescriptor descriptor;descriptor.length=length;descriptor.storage=ALMetalStorageMode::shared;descriptor.label=label;
    result->value=ALMetalBuffer::create(*context->device,descriptor,error);
    if(!result->value)return {};
    result->budget=context->budget;result->charged_bytes=length;
    context->budget->bytes.fetch_add(length,std::memory_order_relaxed);
    return result;
}
std::shared_ptr<Buffer> makePhysicalBuffer(const std::shared_ptr<Context>& context,std::size_t length,
    const char* label,std::string* error)
{
    if(length>std::numeric_limits<std::size_t>::max()-4095)return {};
    auto buffer=makeBuffer(context,(length+4095)&~std::size_t(4095),label,error);if(!buffer)return {};
    const auto actual=[(__bridge id<MTLBuffer>)buffer->value->nativeHandle() allocatedSize];
    if(!actual||actual<buffer->charged_bytes)return {};
    const auto extra=actual-buffer->charged_bytes,live=context->budget->bytes.load(std::memory_order_relaxed);
    if(live>context->budget->limit||extra>context->budget->limit-live)return {};
    context->budget->bytes.fetch_add(extra,std::memory_order_relaxed);buffer->charged_bytes=actual;return buffer;
}
bool finite(const float* values,std::size_t count)
{for(std::size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
struct SourceInput
{
    Signature signature;
    std::shared_ptr<const ALMetalWorldMaterialSource> material;
    std::shared_ptr<const ALMetalWorldResourceSource> resources;
    std::array<std::shared_ptr<const ALMetalBuffer>,3> numeric;
    const ALMetalBuffer* vertices=nullptr;
    const ALMetalBuffer* indices=nullptr;
    std::size_t vertex_count=0,index_count=0,index_offset=0;
    ALMetalIndexType index_type=ALMetalIndexType::uint16;
    bool pbr=false;
};
bool captureInput(const ALMetalWorldSourceRequest& request,const ALMetalWorldSourceRegion& region,
                  const ALMetalWorldSourceGlobals& globals,SourceInput& input,std::string* error,
                  std::shared_ptr<const ALMetalWorldResourceSource> resources={},bool certified=false)
{
    if(!request.source||!request.native.owner)return false;
    const auto& source=*request.source;
    if(!resources)resources=source.native_resource_source;
    const bool pbr=source.pbr_slot_count!=0,material=source.material_slot_count!=0;
    if(pbr==material||source.normal_texture||source.skin_palette_record||source.body_palette_record||source.skin_palette||source.skin_joint_count||source.weight_offset||
       source.avatar_rigid||source.avatar_skinning||source.avatar_cloth||source.terrain||source.terrain_pass||source.impostor||
       !source.native_material_source||!source.native_material_source->matches(source)||
       !resources||!resources->matches(source)||
       (request.native.vertices&&request.native.vertices.get()!=source.vertices)||
       (request.native.indices&&request.native.indices.get()!=source.indices))
    {fail(error,"Native source request lacks an exact static material generation");return false;}
    ALMetalWorldViewMatrices matrices;
    std::memcpy(matrices.modelview_projection,region.modelview_projection,64);
    std::memcpy(matrices.modelview,region.modelview,64);std::memcpy(matrices.normal_matrix,region.normal_matrix,48);
    for(unsigned axis=0;axis<3;++axis)matrices.rigged_normal_matrix[axis][axis]=1;
    std::memcpy(matrices.texture_matrix,source.texture_matrix,64);
    ALMetalWorldGBufferScene validation;validation.normal_hdr=globals.flags[2];validation.width=validation.height=1;
    if(!certified && !alMetalValidateWorldBatch(validation,ALMetalWorldDraw(source,&matrices),error))return false;
    input.material=source.native_material_source;input.resources=std::move(resources);input.pbr=pbr;
    input.numeric={input.material->materialLease(),input.material->pbrLease(false),input.material->pbrLease(true)};
    input.vertices=source.vertices;input.indices=source.indices;input.vertex_count=source.vertex_count;
    input.index_count=source.index_count;input.index_offset=source.index_offset;input.index_type=source.index_type;
    input.signature.offsets={source.position_offset,source.normal_offset,source.texcoord_offset,source.color_offset,
        pbr||source.materialSlots()[0].normal_texture?source.tangent_offset:source.normal_offset,
        material&&source.materialSlots()[0].normal_texture?source.texcoord1_offset:source.texcoord_offset,
        material&&source.materialSlots()[0].specular_texture?source.texcoord2_offset:source.texcoord_offset};
    for(unsigned binding=0;binding<7;++binding)input.signature.handles[binding]=source.vertices->nativeHandle();
    for(unsigned binding=0;binding<3;++binding)input.signature.handles[7+binding]=input.numeric[binding]?input.numeric[binding]->nativeHandle():nullptr;
    for(unsigned value=0;value<16;++value)input.signature.values[value]=std::bit_cast<std::uint32_t>(source.texture_matrix[value]);
    input.signature.values[16]=std::bit_cast<std::uint32_t>(source.alpha_cutoff);
    input.signature.values[17]=std::bit_cast<std::uint32_t>(source.use_vertex_color?1.f:0.f);
    input.signature.values[18]=std::bit_cast<std::uint32_t>(source.alpha_cutoff_bias?1.f:0.f);
    input.signature.values[19]=std::bit_cast<std::uint32_t>(source.alpha_mask?1.f:0.f);input.signature.values[20]=source.texture_count;
    return true;
}
bool sameInput(const SourceInput& a,const SourceInput& b)
{
    return a.signature==b.signature&&a.material==b.material&&a.resources==b.resources&&a.vertices==b.vertices&&a.indices==b.indices&&
        a.vertex_count==b.vertex_count&&a.index_count==b.index_count&&a.index_offset==b.index_offset&&a.index_type==b.index_type&&a.pbr==b.pbr;
}
}

std::uint64_t ALMetalWorldSourcePublicationWork::chargedBytes()const noexcept
{
    std::uint64_t total=0;
    for(auto bytes:{encoded_row_bytes,encoded_table_bytes,encoded_bank_range_bytes,encoded_mapping_bytes,copied_page_bytes,copied_bank_bytes,zeroed_page_bytes,zeroed_bank_bytes})
    {if(bytes>std::numeric_limits<std::uint64_t>::max()-total)return std::numeric_limits<std::uint64_t>::max();total+=bytes;}
    return total;
}

struct ALMetalWorldSourceCommandMembership::Impl
{
    void* device=nullptr;
    std::vector<ALMetalWorldSourceIndexedCall> calls;
    std::vector<RP::View> index_views;
    bool index_complete=true;
    std::vector<ALMetalWorldSourceCommandRun> runs;
    std::vector<ALMetalWorldSourceDirectBinding> bindings;
    std::vector<ALMetalWorldSourceNativeFootprint> assets;
    void* policy=nullptr;
};
ALMetalWorldSourceCommandMembership::ALMetalWorldSourceCommandMembership(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceCommandMembership::~ALMetalWorldSourceCommandMembership()=default;
std::span<const ALMetalWorldSourceIndexedCall> ALMetalWorldSourceCommandMembership::calls()const noexcept{return mImpl->calls;}
std::span<const RP::View> ALMetalWorldSourceCommandMembership::indexResourceViews()const noexcept{return mImpl->index_views;}
bool ALMetalWorldSourceCommandMembership::indexResourcesComplete()const noexcept{return mImpl->index_complete;}
std::span<const ALMetalWorldSourceCommandRun> ALMetalWorldSourceCommandMembership::runs()const noexcept{return mImpl->runs;}
void* ALMetalWorldSourceCommandMembership::nativeDevice()const noexcept{return mImpl->device;}
std::span<const ALMetalWorldSourceDirectBinding> ALMetalWorldSourceCommandMembership::directBindings()const noexcept{return mImpl->bindings;}
std::span<const ALMetalWorldSourceNativeFootprint> ALMetalWorldSourceCommandMembership::directAssets()const noexcept{return mImpl->assets;}
void* ALMetalWorldSourceCommandMembership::directPolicyBuffer()const noexcept{return mImpl->policy;}
namespace {struct DirectSlotReader{std::shared_ptr<const ALMetalWorldSourceDirectSlot> slot;};}
struct ALMetalWorldSourceDirectSlot::Impl
{
    std::shared_ptr<Buffer> buffer;
    std::weak_ptr<DirectSlotReader> reader;
    std::shared_ptr<DirectSlotAccounting> accounting;
    std::size_t regions=0,native_bytes=0;
    ~Impl(){if(accounting){accounting->count.fetch_sub(1,std::memory_order_relaxed);accounting->bytes.fetch_sub(native_bytes,std::memory_order_relaxed);}}
};
ALMetalWorldSourceDirectSlot::ALMetalWorldSourceDirectSlot(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceDirectSlot::~ALMetalWorldSourceDirectSlot()=default;
void* ALMetalWorldSourceDirectSlot::nativeBuffer()const noexcept{return mImpl->buffer->value->nativeHandle();}
const RP::View& ALMetalWorldSourceDirectSlot::resourceView()const noexcept{return mImpl->buffer->value->resourceView();}
std::size_t ALMetalWorldSourceDirectSlot::regionOffset(std::uint32_t index)const noexcept{return std::size_t(index)*256;}
std::size_t ALMetalWorldSourceDirectSlot::globalsOffset()const noexcept{return mImpl->regions*256;}
namespace
{
// This cache closure is native-only. In particular it does not keep a cohort,
// source wrapper, C++ generation lease or actual page reader alive.
struct CohortBinding
{
    struct PageProof {std::uint32_t page;};
    struct Asset {void* allocation;std::uint64_t bytes;};
    std::shared_ptr<const ALMetalWorldSourceCommandMembership> membership;
    std::vector<PageProof> pages;
    std::vector<void*> geometry,numeric;
    std::vector<RP::View> plan_views;
    bool plan_complete=true;
    std::vector<std::uint32_t> native_owner_indices;
    std::vector<Asset> assets;
    bool asset_footprint_known=true;
    std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> tables;
    std::shared_ptr<const DeclarationMetadata> declaration_metadata;
    std::shared_ptr<const ALMetalWorldResourceDeclaration> declaration;
    std::shared_ptr<Buffer> mapping;
    std::shared_ptr<Buffer> direct_policy;
    mutable std::vector<std::shared_ptr<ALMetalWorldSourceDirectSlot>> direct_slots;
    std::size_t metadata=0;
};
}

struct ALMetalWorldAnimatedGpuSource::Impl
{
    std::weak_ptr<const ALMetalWorldAnimatedSource> source;
    std::weak_ptr<const void> native;
    const void* native_pointer=nullptr;
    std::shared_ptr<ALMetalWorldResources> resources;
    ALMetalWorldAnimatedNativeBinding binding;
    std::shared_ptr<Buffer> policy,bones;
    std::shared_ptr<const ALMetalWorldResourceTable> table;
    std::vector<ALMetalWorldSourceNativeFootprint> assets;
    std::vector<RP::View> plan_views;
    bool plan_complete=true;
    std::shared_ptr<const DeclarationMetadata> graph_metadata;
};
ALMetalWorldAnimatedGpuSource::ALMetalWorldAnimatedGpuSource(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldAnimatedGpuSource::~ALMetalWorldAnimatedGpuSource()=default;
bool ALMetalWorldAnimatedGpuSource::currentGeneration()const noexcept
{return !mImpl->source.expired()&&!mImpl->native.expired();}
bool ALMetalWorldAnimatedGpuSource::matchesNativeLease(const std::shared_ptr<const void>& native)const noexcept
{return native && native.get()==mImpl->native_pointer && !mImpl->native.owner_before(native) && !native.owner_before(mImpl->native);}
bool ALMetalWorldAnimatedGpuSource::declareResources(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,std::string* error)const
{
    if(mImpl->plan_complete)
    {
        for(const auto& view:mImpl->plan_views)if(!encoder.declareResourceUse(view,RP::Access::read,error))return false;
    }
    else (void)encoder.nativeHandleForOpaqueResources(); // Conservative legacy closure.
    return mImpl->resources->declareResources(encoder,command,mImpl->table,error);
}
bool ALMetalWorldAnimatedGpuSource::declareResourceUses(ALMetalComputeEncoder& encoder,std::string* error)const
{
    if(!mImpl->plan_complete){(void)encoder.nativeHandleForOpaqueResources();return true;}
    for(const auto& view:mImpl->plan_views)if(!encoder.declareResourceUse(view,RP::Access::read,error))return false;
    return encoder.declareResourceUse(mImpl->table->argumentLease()->resourceView(),RP::Access::read,error);
}

const ALMetalWorldAnimatedNativeBinding& ALMetalWorldAnimatedGpuSource::binding()const noexcept{return mImpl->binding;}
std::shared_ptr<const ALMetalWorldResourceTable> ALMetalWorldAnimatedGpuSource::table()const noexcept{return mImpl->table;}
std::span<const ALMetalWorldSourceNativeFootprint> ALMetalWorldAnimatedGpuSource::assets()const noexcept{return mImpl->assets;}

struct ALMetalWorldSourceFrameView::Impl
{
    struct CommandSource
    {
        ALMetalWorldSourceKey key;
        const ALMetalBuffer* indices=nullptr;
        void* native_indices=nullptr;
    };
    std::shared_ptr<Context> context;
    std::shared_ptr<const CohortBinding> cohort;
    std::shared_ptr<BankReader> bank;
    std::vector<std::shared_ptr<PageReader>> pages;
    std::shared_ptr<DirectSlotReader> direct_reader;
    bool direct_normal_hdr=false;
    void* direct_command=nullptr;
    std::shared_ptr<Buffer> regions,globals,mapping;
    // Retain only the native resource. FrameData owns/recycles its C++ wrapper
    // and may die independently; no raw slice pointer escapes publication.
    id<MTLBuffer> arena_buffer=nil;
    RP::View arena_plan;
    std::vector<RP::View> plan_views;
    bool plan_complete=true;
    std::size_t arena_region_offset=0,arena_globals_offset=0;
    std::weak_ptr<const ALMetalCommandCompletion> arena_command;
    bool publishView(ALMetalCommandBuffer* command,ALMetalWorldFrameData* frame_data,
        std::span<const ALMetalWorldSourceRegion> values,const ALMetalWorldSourceGlobals& flags,std::string* error)
    {
        if(frame_data)
        {
            if(!command || values.empty() || values.size()>alMetalWorldSourceMaximumRows)return false;
            auto origin=command->completionToken();
            if(!origin || origin->isComplete())return false;
            const auto globals_offset=(values.size_bytes()+255)&~std::size_t(255);
            const auto length=globals_offset+sizeof(flags);
            std::array<std::byte,288> small{};std::vector<std::byte> large;
            if(length>small.size())large.resize(length);
            auto* bytes=length<=small.size()?small.data():large.data();
            std::memcpy(bytes,values.data(),values.size_bytes());
            std::memcpy(bytes+globals_offset,&flags,sizeof(flags));
            ALMetalWorldDataSlice slice;
            if(!frame_data->publish(*command,bytes,length,slice,error,false))return false;
            if(!slice.buffer || slice.offset%256 || slice.length!=length ||
               slice.offset>slice.buffer->length() || length>slice.buffer->length()-slice.offset)
            {fail(error,"Native cohort arena publication returned an invalid range");return false;}
            arena_buffer=(__bridge id<MTLBuffer>)slice.buffer->nativeHandle();
            arena_plan=slice.buffer->resourceView();
            if(!arena_buffer){fail(error,"Native cohort arena publication has no native buffer");return false;}
            arena_region_offset=slice.offset;arena_globals_offset=slice.offset+globals_offset;arena_command=origin;
            return true;
        }
        regions=makeBuffer(context,values.size_bytes(),"Alchemy source regional view matrices",error);
        globals=makeBuffer(context,sizeof(flags),"Alchemy source view globals",error);
        if(!regions || !globals)return false;
        std::memcpy(regions->value->mappedBytes(),values.data(),values.size_bytes());
        std::memcpy(globals->value->mappedBytes(),&flags,sizeof(flags));return true;
    }
    std::vector<std::uint32_t> rows,generations;
    std::vector<ALMetalWorldSourceCohortRange> ranges;
    std::vector<CommandSource> commands;
    std::vector<std::shared_ptr<const void>> native_owners;
    std::vector<std::shared_ptr<const ALMetalBuffer>> buffers;
    std::vector<std::shared_ptr<const ALMetalTexture>> textures;
    std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> tables;
    std::vector<void*> geometry;
    std::weak_ptr<const void> declared_encoder;
    std::optional<std::size_t> direct_failure_after;
};
ALMetalWorldSourceFrameView::ALMetalWorldSourceFrameView(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceFrameView::~ALMetalWorldSourceFrameView()=default;
std::size_t ALMetalWorldSourceFrameView::size()const noexcept
{return mImpl->cohort?mImpl->cohort->membership->calls().size():mImpl->rows.size();}
std::span<const ALMetalWorldSourceCohortRange> ALMetalWorldSourceFrameView::cohortRanges()const noexcept{return mImpl->ranges;}
bool ALMetalWorldSourceFrameView::directBindings()const noexcept{return bool(mImpl->direct_reader);}
bool ALMetalWorldSourceFrameView::directNormalHDR()const noexcept{return mImpl->direct_normal_hdr;}
std::uint64_t ALMetalWorldSourceFrameView::directBindingAssetLimit()const noexcept{return mImpl->context->direct_asset_limit;}
bool ALMetalWorldSourceFrameView::matchesDirectCommand(const ALMetalCommandBuffer& command)const noexcept
{
    if(!mImpl->direct_reader)return true;
    auto origin=mImpl->arena_command.lock();
    return origin&&!origin->isComplete()&&!command.isCommitted()&&command.nativeIdentity()==mImpl->direct_command;
}
std::shared_ptr<const ALMetalWorldSourceDirectSlot> ALMetalWorldSourceFrameView::directBindingSlot()const noexcept
{return mImpl->direct_reader?mImpl->direct_reader->slot:nullptr;}
std::span<const std::shared_ptr<const ALMetalWorldResourceTable>> ALMetalWorldSourceFrameView::directBindingTables()const noexcept
{return mImpl->cohort?std::span(mImpl->cohort->tables):std::span<const std::shared_ptr<const ALMetalWorldResourceTable>>{};}
std::shared_ptr<const ALMetalWorldSourceCommandMembership> ALMetalWorldSourceFrameView::commandMembership()const noexcept
{return mImpl->cohort?mImpl->cohort->membership:nullptr;}
std::uint32_t ALMetalWorldSourceFrameView::rowId(std::size_t index)const noexcept
{return mImpl->cohort?(index<size()?mImpl->cohort->membership->calls()[index].row_id:std::numeric_limits<std::uint32_t>::max()):
    (index<mImpl->rows.size()?mImpl->rows[index]:std::numeric_limits<std::uint32_t>::max());}
std::uint32_t ALMetalWorldSourceFrameView::allocationGeneration(std::size_t index)const noexcept
{return mImpl->cohort?(index<size()?mImpl->cohort->membership->calls()[index].row_generation:0):
    (index<mImpl->generations.size()?mImpl->generations[index]:0);}
bool ALMetalWorldSourceFrameView::matchesSourceCommand(std::size_t index,const ALMetalWorldSourceKey& key,
                                                     const ALMetalBuffer& indices)const noexcept
{
    if(mImpl->cohort)
    {
        if(index>=size())return false;
        const auto& source=mImpl->cohort->membership->calls()[index];
        return source.key.identity==key.identity&&source.key.flavor==key.flavor&&source.key.revision==key.revision&&
            source.native_indices==indices.nativeHandle();
    }
    if(index>=mImpl->commands.size())return false;
    const auto& source=mImpl->commands[index];
    return source.key.identity==key.identity&&source.key.flavor==key.flavor&&source.key.revision==key.revision&&
        source.indices==&indices&&source.native_indices==indices.nativeHandle();
}
std::weak_ptr<const ALMetalWorldResourceTable> ALMetalWorldSourceFrameView::tableGenerationForTesting(std::size_t index)const noexcept
{const auto& tables=mImpl->cohort?mImpl->cohort->tables:mImpl->tables;
    return index<tables.size()?std::weak_ptr<const ALMetalWorldResourceTable>(tables[index]):std::weak_ptr<const ALMetalWorldResourceTable>{};}
bool ALMetalWorldSourceFrameView::bind(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,std::string* error)const
try
{
    auto& view=*mImpl;
    if(command.isCommitted()||command.nativeDevice()!=(__bridge void*)view.context->native ||
       encoder.nativeCommandBufferIdentity()!=command.nativeIdentity())
    {fail(error,"Native source view requires its exact uncommitted device command");return false;}
    if(view.arena_buffer||view.direct_reader)
    {
        const auto origin=view.arena_command.lock();const auto current=command.completionToken();
        if(!origin || origin->isComplete() || !current || origin.get()!=current.get() ||
           origin.owner_before(current)||current.owner_before(origin))
        {fail(error,"Native cohort arena view requires its exact incomplete publication command");return false;}
    }
    const auto identity=encoder.declarationIdentity(true);
    if(!identity){fail(error,"Native source view requires an active encoder identity");return false;}
    const bool new_declaration=view.declared_encoder.owner_before(identity)||identity.owner_before(view.declared_encoder);
    const auto& captured=view.cohort?view.cohort->plan_views:view.plan_views;
    bool complete=view.cohort?view.cohort->plan_complete:view.plan_complete;
    const auto use=[&](const RP::View& resource)
    {if(!resource){complete=false;return true;}return encoder.declareResourceUse(resource,RP::Access::read,error);};
    if(complete&&new_declaration)
    {
        for(const auto& resource:captured)if(!use(resource))return false;
        if(view.direct_reader)
        {if(!use(view.direct_reader->slot->resourceView())||!use(view.cohort->direct_policy->value->resourceView()))return false;}
        else
        {
            if(!use(view.bank->bank->buffer->value->resourceView()))return false;
            for(const auto& page:view.pages)if(!use(page->page->buffer->value->resourceView()))return false;
            if(view.arena_buffer){if(!use(view.arena_plan))return false;}
            else if(!use(view.regions->value->resourceView())||!use(view.globals->value->resourceView()))return false;
            if(!use(view.mapping->value->resourceView()))return false;
        }
    }
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)
        (complete?encoder.nativeHandleForDeclaredResources():encoder.nativeHandleForOpaqueResources());
    if(!native){fail(error,"Native source view requires a live render encoder");return false;}
    const auto declare=[&](const ALMetalBuffer& buffer,MTLRenderStages stages)
    {[native useResource:(__bridge id<MTLBuffer>)buffer.nativeHandle() usage:MTLResourceUsageRead stages:stages];};
    if(new_declaration)
    {
        if(!command.retainUntilCompletion(shared_from_this(),error))return false;
        if(!view.direct_reader)
        {
            declare(*view.bank->bank->buffer->value,MTLRenderStageVertex|MTLRenderStageFragment);
            for(const auto& page:view.pages)declare(*page->page->buffer->value,MTLRenderStageVertex|MTLRenderStageFragment);
        }
        else
        {
            [native useResource:(__bridge id<MTLBuffer>)view.direct_reader->slot->nativeBuffer() usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
            declare(*view.cohort->direct_policy->value,MTLRenderStageVertex);
        }
        for(const auto& buffer:view.buffers)declare(*buffer,MTLRenderStageVertex|MTLRenderStageFragment);
        const auto& geometry_handles=view.cohort?view.cohort->geometry:view.geometry;
        if(!geometry_handles.empty())
        {
            const auto* geometry=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(geometry_handles.data());
            [native useResources:geometry count:geometry_handles.size() usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        }
        if(view.cohort && !view.cohort->numeric.empty())
        {
            const auto* numeric=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(view.cohort->numeric.data());
            [native useResources:numeric count:view.cohort->numeric.size() usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
        }
        if(view.cohort)
        {
            if(!view.cohort->declaration || !view.context->resources->declareCertifiedResources(
                encoder,command,*view.cohort->declaration,error))return false;
            view.context->cohort_declaration_calls.fetch_add(1,std::memory_order_relaxed);
            view.context->cohort_declared_resources.fetch_add(view.cohort->declaration->resourceCount(),std::memory_order_relaxed);
        }
        else for(const auto& table:view.tables)
            if(!view.context->resources->declareResources(encoder,command,table,error))return false;
        view.declared_encoder=identity;
    }
    if(view.direct_reader){view.context->binds.fetch_add(1,std::memory_order_relaxed);return true;}
    if(view.arena_buffer)
    {
        // Flush any queued direct draw before native binding changes, and
        // leave these slots conservatively unknown to the wrapper's cache.
        // Subsequent ordinary bridge binds therefore cannot be skipped.
        encoder.invalidateIndirectBufferBindings();
        [native setVertexBuffer:view.arena_buffer offset:view.arena_region_offset atIndex:1];
        [native setVertexBuffer:view.arena_buffer offset:view.arena_globals_offset atIndex:2];
        [native setFragmentBuffer:view.arena_buffer offset:view.arena_globals_offset atIndex:2];
    }
    const bool bound=encoder.setVertexBuffer(0,*view.bank->bank->buffer->value,0,error) &&
        encoder.setFragmentBuffer(0,*view.bank->bank->buffer->value,0,error) &&
        (view.arena_buffer || (encoder.setVertexBuffer(1,*view.regions->value,0,error) &&
            encoder.setVertexBuffer(2,*view.globals->value,0,error) && encoder.setFragmentBuffer(2,*view.globals->value,0,error))) &&
        encoder.setVertexBuffer(3,*view.mapping->value,0,error);
    if(bound)view.context->binds.fetch_add(1,std::memory_order_relaxed);
    return bound;
}
catch(const std::bad_alloc&){if(error)error->clear();return false;}

void ALMetalWorldSourceFrameView::failDirectRangeAfterForTesting(std::size_t draws)const noexcept
{mImpl->direct_failure_after=draws;}

ALMetalWorldSourceFrameView::DirectExecution ALMetalWorldSourceFrameView::drawDirectRange(
    ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    std::size_t first,std::size_t count,std::string* error)const
{
    DirectExecution result;
    std::uint64_t elements=0;
    const auto reject=[&](const char* message)
    {
        fail(error,message);
        if(result.draws_encoded)encoder.recordPrevalidatedDraws(static_cast<std::uint32_t>(result.draws_encoded),elements);
        return result;
    };
    try
    {
        if(!mImpl->cohort||!mImpl->direct_reader||!count||first>size()||count>size()-first||
           !matchesDirectCommand(command)||command.isCommitted()||!encoder.canExecuteStaticDraws()||
           encoder.nativeDeviceForIndirect()!=command.nativeDevice()||
           encoder.nativeCommandBufferIdentity()!=command.nativeIdentity())
            return reject("Direct source range requires its exact active command and native pipeline");
        const auto& membership=*mImpl->cohort->membership;
        const auto calls=membership.calls();const auto bindings=membership.directBindings();
        const auto slot=directBindingSlot();
        if(bindings.size()!=calls.size()||!slot||!slot->nativeBuffer()||!membership.directPolicyBuffer())
            return reject("Direct source range has incomplete published bindings");
        const bool one_raster_run=std::any_of(membership.runs().begin(),membership.runs().end(),[&](const auto& run)
            {return first>=run.first&&first-run.first<=run.count&&count<=run.count-(first-run.first);});
        if(!one_raster_run)return reject("Direct source range crosses a published raster-state boundary");
        for(std::size_t at=first;at<first+count;++at)
        {
            const auto& call=calls[at];const auto& binding=bindings[at];
            if(!call.native_indices||!call.index_count||!binding.geometry[0]||!binding.resource_table||
               !(binding.legacy_numeric||(directNormalHDR()?binding.pbr_hdr:binding.pbr_ldr)))
                return reject("Direct source range contains an invalid published indexed call");
        }
        // Retain and declare the complete native-only closure before changing
        // draw state. The exact FrameView owns the overwrite-prevention token.
        if(!bind(encoder,command,error))return result;
        encoder.invalidateIndirectBufferBindings();
        id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForDeclaredResources();
        if(!native)return reject("Direct source range encoder ended before emission");
        result.native_prefix=true;
        for(std::size_t at=first;at<first+count;++at)
        {
            if(mImpl->direct_failure_after&&*mImpl->direct_failure_after==0)
            {
                mImpl->direct_failure_after.reset();
                return reject("Forced direct source failure after a native prefix");
            }
            const auto& call=calls[at];const auto& binding=bindings[at];
            constexpr unsigned slots[]{0,1,2,3,8,9,10};
            for(unsigned attribute=0;attribute<7;++attribute)
                [native setVertexBuffer:(__bridge id<MTLBuffer>)binding.geometry[attribute]
                    offset:binding.offsets[attribute] atIndex:slots[attribute]];
            id<MTLBuffer> view=(__bridge id<MTLBuffer>)slot->nativeBuffer();
            [native setVertexBuffer:view offset:slot->regionOffset(binding.region) atIndex:4];
            [native setVertexBuffer:view offset:slot->globalsOffset() atIndex:5];
            [native setVertexBuffer:(__bridge id<MTLBuffer>)membership.directPolicyBuffer() offset:binding.policy_offset atIndex:6];
            void* numeric=binding.legacy_numeric?binding.legacy_numeric:(directNormalHDR()?binding.pbr_hdr:binding.pbr_ldr);
            if(!binding.legacy_numeric)[native setVertexBuffer:(__bridge id<MTLBuffer>)numeric offset:0 atIndex:11];
            [native setFragmentBuffer:(__bridge id<MTLBuffer>)numeric offset:0 atIndex:0];
            [native setFragmentBuffer:(__bridge id<MTLBuffer>)binding.resource_table offset:0 atIndex:1];
            [native drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:call.index_count
                indexType:call.index_type==ALMetalIndexType::uint16?MTLIndexTypeUInt16:MTLIndexTypeUInt32
                indexBuffer:(__bridge id<MTLBuffer>)call.native_indices indexBufferOffset:call.index_offset
                instanceCount:1 baseVertex:0 baseInstance:call.row_id];
            ++result.draws_encoded;elements+=call.index_count;
            if(mImpl->direct_failure_after)--*mImpl->direct_failure_after;
        }
        encoder.recordPrevalidatedDraws(static_cast<std::uint32_t>(result.draws_encoded),elements);
        result.success=true;return result;
    }
    catch(const std::bad_alloc&)
    {
        if(error)error->clear();
        if(result.draws_encoded)encoder.recordPrevalidatedDraws(static_cast<std::uint32_t>(result.draws_encoded),elements);
        return result;
    }
}

struct ALMetalWorldSourcePages::Impl
{
    struct DeviceProof
    {
        __weak id<MTLBuffer> buffer=nil;
        std::list<void*>::iterator lru;
    };
    std::shared_ptr<Context> context;
    ALMetalWorldSourcePageLimits limits;
    std::vector<Row> rows;
    std::vector<std::uint32_t> free;
    std::unordered_map<Key,std::uint32_t,KeyHash> lookup;
    std::unordered_map<std::uint64_t,std::vector<std::uint32_t>> identities;
    std::list<std::uint32_t> lru;
    // No strong native/C++ generation ownership. A zeroed weak object rejects
    // native-address reuse; a live MTLBuffer's device is immutable. Five is
    // the maximum vertex/index/legacy/PBR-LDR/PBR-HDR union per source row.
    std::unordered_map<void*,DeviceProof> device_proofs;
    std::list<void*> device_proof_lru;
    std::array<std::shared_ptr<Page>,alMetalWorldSourceMaximumRows/alMetalWorldSourceRowsPerPage> pages;
    std::shared_ptr<Bank> bank;
    std::shared_ptr<Buffer> cohort_mapping;
    struct CohortEntry
    {
        struct Owner
        {
            std::weak_ptr<const ALMetalWorldSourceCohort> cohort;
            std::weak_ptr<const void> cpu,native;
            const void* cpu_pointer=nullptr;const void* native_pointer=nullptr;
        };
        std::vector<Owner> owners;
        std::size_t hash=0;
        std::shared_ptr<const CohortBinding> binding;
        CohortValidity validity;
        // Exactly one node per numeric call, bounded by cached_cohort_rows.
        // Array destruction unlinks before validity/binding are destroyed.
        std::unique_ptr<CohortRowWatch[]> watches;
    };
    // Declared after fixed rows: node destructors always see live Row heads,
    // including ordinary Impl destruction and failed staging/clear paths.
    std::list<CohortEntry> cohorts;
    std::size_t cohort_rows=0,cohort_metadata=0,cohort_references=0;
    struct AssetRef {std::uint64_t bytes;std::size_t references=0;bool staged=false;};
    std::unordered_map<void*,AssetRef> cohort_assets;
    std::uint64_t cohort_asset_bytes=0;
    bool animated_preparing=false;
    struct AnimatedEntry
    {
        std::weak_ptr<const ALMetalWorldAnimatedSource> source;
        std::weak_ptr<const void> native;
        std::shared_ptr<const ALMetalWorldAnimatedGpuSource> gpu;
    };
    std::unordered_map<const ALMetalWorldAnimatedSource*,AnimatedEntry> animated;
    ALMetalWorldSourcePageStats statistics;
    std::uint64_t serial=0;
    bool supported=false,force_failure=false,force_cache_failure=false;
    std::shared_ptr<ALMetalWorldSourcePages::PublicationAdmission> admission;
    std::uint64_t epoch=0;

    void eraseAnimated(std::unordered_map<const ALMetalWorldAnimatedSource*,AnimatedEntry>::iterator entry)noexcept
    {
        for(const auto& asset:entry->second.gpu->assets())
        {
            const auto found=cohort_assets.find(asset.allocation);
            if(found!=cohort_assets.end() && --found->second.references==0)
            {cohort_asset_bytes-=found->second.bytes;if(!found->second.staged)cohort_assets.erase(found);}
        }
        animated.erase(entry);
    }
    void pruneAnimated()noexcept
    {
        for(auto it=animated.begin();it!=animated.end();)
            if(it->second.source.expired()||it->second.native.expired())
            {auto old=it++;eraseAnimated(old);}else ++it;
    }

    void changed(std::size_t slot)noexcept
    {
        // First mark without erasing: one entry can have multiple nodes on
        // this row. Pruning then unlinks whole entries before native rewrite.
        for(auto* watch=rows[slot].watchers;watch;watch=watch->next)watch->entry->valid=false;
        pruneCohorts();
    }
    bool current(const CohortEntry& entry)const noexcept{return entry.validity.valid;}
    std::uint64_t eraseCohort(std::list<CohortEntry>::iterator entry)noexcept
    {
        std::uint64_t released=0;
        for(const auto& asset:entry->binding->assets)
        {
            const auto found=cohort_assets.find(asset.allocation);
            if(found!=cohort_assets.end() && --found->second.references==0)
            {
                cohort_asset_bytes-=found->second.bytes;
                // Staged incoming membership will acquire this same asset;
                // its prospective unique footprint therefore did not shrink.
                if(!found->second.staged){released+=found->second.bytes;cohort_assets.erase(found);}
            }
        }
        cohort_rows-=entry->binding->membership->calls().size();cohort_metadata-=entry->binding->metadata;
        cohort_references-=entry->owners.size();cohorts.erase(entry);
        return released;
    }
    void pruneCohorts()noexcept
    {
        for(auto it=cohorts.begin();it!=cohorts.end();)
            if(!current(*it)||std::any_of(it->owners.begin(),it->owners.end(),[](const auto& owner)
                {return owner.cohort.expired()||owner.cpu.expired()||owner.native.expired();}))
                {auto old=it++;eraseCohort(old);}else ++it;
    }
    bool proveDevice(const ALMetalBuffer& buffer,std::string* error)
    {
        void* const handle=buffer.nativeHandle();
        id<MTLBuffer> native=(__bridge id<MTLBuffer>)handle;
        auto found=device_proofs.find(handle);
        if(found!=device_proofs.end())
        {
            id<MTLBuffer> proven=found->second.buffer;
            if(proven && proven==native)
            {
                device_proof_lru.splice(device_proof_lru.end(),device_proof_lru,found->second.lru);
                ++statistics.device_proof_hits;return true;
            }
            device_proof_lru.erase(found->second.lru);device_proofs.erase(found);
        }
        ++statistics.device_checks;
        if(!native || native.device!=context->native)
        {
            ++statistics.device_rejections;
            fail(error,"Native source buffer belongs to a different Metal device");return false;
        }
        const auto capacity=limits.maximum_rows*5;
        if(device_proofs.size()>=capacity)
        {
            device_proofs.erase(device_proof_lru.front());device_proof_lru.pop_front();
        }
        device_proof_lru.push_back(handle);
        try{device_proofs.emplace(handle,DeviceProof{native,std::prev(device_proof_lru.end())});}
        catch(...){device_proof_lru.pop_back();throw;}
        return true;
    }

    bool preparePage(std::uint32_t page_index,std::string* error)
    {
        auto page=pages[page_index];
        const bool page_copy=page && !page->reader.expired();
        if(!page || page_copy)
        {
            auto next=std::make_shared<Page>();
            next->buffer=makeBuffer(context,context->stride*alMetalWorldSourceRowsPerPage,"Alchemy source argument page",error);
            if(!next->buffer)return false;
            if(page)
            {
                std::memcpy(next->buffer->value->mappedBytes(),page->buffer->value->mappedBytes(),next->buffer->value->length());
                statistics.copied_page_bytes+=next->buffer->value->length();
            }
            else std::memset(next->buffer->value->mappedBytes(),0,next->buffer->value->length());
            auto next_bank=bank;
            if(!bank || !bank->reader.expired())
            {
                next_bank=std::make_shared<Bank>();
                next_bank->buffer=makeBuffer(context,context->bank_length,"Alchemy source pointer bank",error);
                if(!next_bank->buffer)return false;
                if(bank)
                {
                    std::memcpy(next_bank->buffer->value->mappedBytes(),bank->buffer->value->mappedBytes(),context->bank_length);
                    statistics.copied_bank_bytes+=context->bank_length;
                }
                else std::memset(next_bank->buffer->value->mappedBytes(),0,context->bank_length);
                ++statistics.bank_allocations;
            }
            [context->bank_encoder setArgumentBuffer:(__bridge id<MTLBuffer>)next_bank->buffer->value->nativeHandle() offset:0];
            for(std::size_t slot=0;slot<alMetalWorldSourceRowsPerPage;++slot)
                [context->bank_encoder setBuffer:(__bridge id<MTLBuffer>)next->buffer->value->nativeHandle()
                    offset:slot*context->stride atIndex:page_index*alMetalWorldSourceRowsPerPage+slot];
            [context->bank_encoder setArgumentBuffer:nil offset:0];
            pages[page_index]=std::move(next);bank=std::move(next_bank);++statistics.page_allocations;
        }
        return true;
    }
};
ALMetalWorldSourcePages::ALMetalWorldSourcePages(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourcePages::~ALMetalWorldSourcePages()=default;
std::shared_ptr<ALMetalWorldSourcePages> ALMetalWorldSourcePages::create(
    const ALMetalDevice& device,const ALMetalShaderLibrary& library,std::uint32_t anisotropy,
    ALMetalWorldSourcePageLimits limits,std::string* error)
try
{
    if(!device.nativeHandle()||!library.nativeHandle()||anisotropy<1||anisotropy>16 ||
       !limits.maximum_rows||limits.maximum_rows>alMetalWorldSourceMaximumRows)
    {fail(error,"Native source page context is invalid");return {};}
    auto impl=std::make_unique<Impl>();impl->context=std::make_shared<Context>();auto& context=*impl->context;
    if(!limits.maximum_cached_cohort_native_asset_bytes)
        limits.maximum_cached_cohort_native_asset_bytes=std::clamp<std::uint64_t>(device.info().recommended_working_set_size/16,
            256ULL*1024*1024,4ULL*1024*1024*1024);
    impl->limits=limits;context.device=&device;context.native=(__bridge id<MTLDevice>)device.nativeHandle();
    context.direct_asset_limit=limits.maximum_cached_cohort_native_asset_bytes;
    context.library=(__bridge id<MTLLibrary>)library.nativeHandle();context.anisotropy=anisotropy;
    if(context.library.device!=context.native || context.native.argumentBuffersSupport!=MTLArgumentBuffersTier2)
    {fail(error,"Native source rows require an exact Tier2 shader context");return {};}
    constexpr const char* names[]{"almetal_world_material_source_rows_vertex","almetal_world_pbr_source_rows_vertex",
        "almetal_world_material_source_rows_fragment","almetal_world_pbr_source_rows_fragment","almetal_world_source_rows_layout"};
    std::array<id<MTLFunction>,5> functions;
    for(std::size_t index=0;index<functions.size();++index)
    {
        functions[index]=[context.library newFunctionWithName:[NSString stringWithUTF8String:names[index]]];
        const auto expected=index<2?MTLFunctionTypeVertex:index<4?MTLFunctionTypeFragment:MTLFunctionTypeKernel;
        if(!functions[index]||functions[index].functionType!=expected)
        {fail(error,"Native source shader functions are unavailable");return {};}
    }
    context.bank_encoder=[functions[0] newArgumentEncoderWithBufferIndex:0];
    // A pointer inside the bank is not itself an argument-buffer argument of
    // that encoder. Reflect the identical row type from its direct ABI kernel.
    context.row_encoder=[functions[4] newArgumentEncoderWithBufferIndex:0];
    const auto length=context.row_encoder.encodedLength,alignment=context.row_encoder.alignment;
    context.bank_length=context.bank_encoder.encodedLength;
    if(!context.bank_encoder||!context.row_encoder||!length||!alignment||!context.bank_encoder.alignment||
       length>std::numeric_limits<std::size_t>::max()-(alignment-1)||!context.bank_length)
    {fail(error,"Native source argument shader ABI is unavailable");return {};}
    context.stride=((length+alignment-1)/alignment)*alignment;
    if(context.stride>std::numeric_limits<std::size_t>::max()/alMetalWorldSourceRowsPerPage)
    {fail(error,"Native source page stride overflows");return {};}
    ALMetalSamplerDescriptor sampler;sampler.max_anisotropy=anisotropy;sampler.support_argument_buffers=true;
    sampler.label="Alchemy source row anisotropic wrap sampler";
    context.sampler=ALMetalSamplerState::create(device,sampler,error);if(!context.sampler)return {};
    ALMetalWorldResourceLimits table_limits;
    // A complete current bank plus an older submitted bank can coexist. Byte
    // capacity remains the hard bound; publisher eviction releases idle tables.
    table_limits.maximum_tables=limits.maximum_rows*2;
    table_limits.maximum_buffer_bytes=std::min<std::uint64_t>(table_limits.maximum_buffer_bytes,limits.maximum_metadata_bytes/4);
    context.budget->limit=limits.maximum_metadata_bytes-table_limits.maximum_buffer_bytes;
    context.resources=ALMetalWorldResources::create(device,library,table_limits,error);if(!context.resources||!context.resources->supported())return {};
    impl->rows.resize(limits.maximum_rows);impl->free.reserve(limits.maximum_rows);
    impl->lookup.reserve(limits.maximum_rows);impl->identities.reserve(limits.maximum_rows);
    for(std::size_t slot=limits.maximum_rows;slot>0;--slot)
    {
        const auto index=static_cast<std::uint32_t>(slot-1);impl->free.push_back(index);
        impl->lru.push_back(index);impl->rows[index].lru=std::prev(impl->lru.end());
    }
    impl->supported=true;
    return std::shared_ptr<ALMetalWorldSourcePages>(new ALMetalWorldSourcePages(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}
bool ALMetalWorldSourcePages::supported()const noexcept{return mImpl->supported;}
bool ALMetalWorldSourcePages::belongsTo(const ALMetalDevice& device,const ALMetalShaderLibrary& library,std::uint32_t anisotropy)const noexcept
{return mImpl->context->device==&device && mImpl->context->native==(__bridge id<MTLDevice>)device.nativeHandle() &&
    mImpl->context->library==(__bridge id<MTLLibrary>)library.nativeHandle() && mImpl->context->anisotropy==anisotropy;}

std::shared_ptr<const ALMetalWorldSourceFrameView> ALMetalWorldSourcePages::freeze(
    std::span<const ALMetalWorldSourceRequest> requests,std::span<const ALMetalWorldSourceRegion> regions,
    const ALMetalWorldSourceGlobals& globals,std::string* error)
{return freezeInternal(nullptr,requests,regions,globals,error);}
std::shared_ptr<const ALMetalWorldSourceFrameView> ALMetalWorldSourcePages::freezeForCommand(
    ALMetalCommandBuffer& command,std::span<const ALMetalWorldSourceRequest> requests,
    std::span<const ALMetalWorldSourceRegion> regions,const ALMetalWorldSourceGlobals& globals,std::string* error)
{return freezeInternal(&command,requests,regions,globals,error);}
std::shared_ptr<const ALMetalWorldSourceFrameView> ALMetalWorldSourcePages::freezeCohortForCommand(
    ALMetalCommandBuffer& command,const ALMetalWorldSourceCohortView& supplied,
    const ALMetalWorldSourceGlobals& globals,std::string* error,ALMetalWorldFrameData* frame_data)
{return freezeCohortsForCommand(command,std::span(&supplied,1),globals,error,frame_data);}
std::shared_ptr<const ALMetalWorldSourceFrameView> ALMetalWorldSourcePages::freezeCohortsForCommand(
    ALMetalCommandBuffer& command,std::span<const ALMetalWorldSourceCohortView> supplied,
    const ALMetalWorldSourceGlobals& supplied_globals,std::string* error,ALMetalWorldFrameData* frame_data,bool direct_bindings)
try
{
    auto& state=*mImpl;++state.statistics.cohort_requests;
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldSourceFrameView>
    {++state.statistics.cohort_failures;fail(error,message);return {};};
    const auto globals=supplied_globals;
    if(!state.supported || command.isCommitted() || command.nativeDevice()!=(__bridge void*)state.context->native ||
       supplied.empty() || supplied.size()>state.limits.maximum_rows || !finite(globals.mirror_clip_plane,4) ||
       globals.flags[0]>1||globals.flags[1]>1||globals.flags[2]>1||globals.flags[3])
        return reject("Native ordered cohort pass shape or command is invalid");
    if(state.force_failure){state.force_failure=false;return reject("Forced native cohort publication failure");}
    struct Selection
    {
        std::shared_ptr<const ALMetalWorldSourceCohort> cohort;
        std::shared_ptr<const void> cpu,native;
        std::uint32_t region=0;
    };
    std::vector<Selection> selections;selections.reserve(supplied.size());
    std::vector<ALMetalWorldSourceCohortRange> ranges;ranges.reserve(supplied.size());
    std::vector<ALMetalWorldSourceRegion> regions;
    std::unordered_map<std::uint64_t,std::uint32_t> region_slots;
    std::size_t total=0,previous_end=0,composition_hash=1469598103934665603ULL;std::uint32_t family=0;
    for(const auto& selected:supplied)
    {
        if(!selected.cohort || !selected.cohort->matchesOwners(selected.cpu_owner,selected.native_owner) ||
           !ALMetalWorldSourceCohort::validView(selected.view) || !selected.cohort->size() ||
           selected.cohort->size()>state.limits.maximum_rows-total || selected.cohort->family()<1 || selected.cohort->family()>2 ||
           (family && family!=selected.cohort->family()) || (!selections.empty()&&selected.logical_begin<previous_end) ||
           selected.logical_begin>std::numeric_limits<std::size_t>::max()-selected.cohort->size())
            return reject("Native ordered cohort pass has incompatible owners, family or logical order");
        family=selected.cohort->family();
        ALMetalWorldSourceRegion region;
        std::memcpy(region.modelview_projection,selected.view.modelview_projection,64);
        std::memcpy(region.modelview,selected.view.modelview,64);std::memcpy(region.normal_matrix,selected.view.normal_matrix,48);
        const auto identity=selected.cohort->regionIdentity();
        const auto [slot,inserted]=region_slots.try_emplace(identity,static_cast<std::uint32_t>(regions.size()));
        if(inserted)regions.push_back(region);
        else if(std::memcmp(&regions[slot->second],&region,sizeof(region)))
            return reject("Native cohort region identity has conflicting per-view transforms");
        selections.push_back({selected.cohort,selected.cpu_owner,selected.native_owner,slot->second});
        ranges.push_back({selected.logical_begin,total,selected.cohort->size()});
        total+=selected.cohort->size();previous_end=selected.logical_begin+selected.cohort->size();
        for(const auto pointer:{static_cast<const void*>(selected.cohort.get()),selected.native_owner.get()})
            composition_hash=(composition_hash^std::hash<const void*>{}(pointer))*1099511628211ULL;
    }
    const auto publishDirect=[&](ALMetalWorldSourceFrameView::Impl& view,const std::shared_ptr<const CohortBinding>& binding)
    {
        auto origin=command.completionToken();if(!origin||origin->isComplete())return false;
        std::shared_ptr<ALMetalWorldSourceDirectSlot> slot;
        for(const auto& candidate:binding->direct_slots)
            if(candidate->mImpl->reader.expired()){slot=candidate;++state.statistics.direct_slot_hits;break;}
        if(!slot)
        {
            if(state.context->direct_slots->count.load(std::memory_order_relaxed)>=128)
            {fail(error,"Native direct binding slot capacity is exhausted");return false;}
            auto storage=std::make_unique<ALMetalWorldSourceDirectSlot::Impl>();storage->regions=regions.size();
            storage->buffer=makePhysicalBuffer(state.context,regions.size()*256+sizeof(globals),"Alchemy fixed-address direct cohort view",error);
            if(!storage->buffer)return false;
            // Stage reader/vector allocations before publishing or charging
            // the new storage. Cached banks never hold this actual reader.
            binding->direct_slots.reserve(binding->direct_slots.size()+1);
            storage->native_bytes=storage->buffer->charged_bytes;
            const auto metadata=sizeof(ALMetalWorldSourceDirectSlot)+sizeof(ALMetalWorldSourceDirectSlot::Impl)+
                sizeof(DirectSlotReader)+8*sizeof(void*)+sizeof(binding->direct_slots[0]);
            const auto used=state.context->budget->bytes.load(std::memory_order_relaxed);
            if(used>state.context->budget->limit||metadata>state.context->budget->limit-used)return false;
            state.context->budget->bytes.fetch_add(metadata,std::memory_order_relaxed);storage->buffer->charged_bytes+=metadata;
            storage->accounting=state.context->direct_slots;
            storage->accounting->count.fetch_add(1,std::memory_order_relaxed);
            storage->accounting->bytes.fetch_add(storage->native_bytes,std::memory_order_relaxed);
            slot=std::shared_ptr<ALMetalWorldSourceDirectSlot>(new ALMetalWorldSourceDirectSlot(std::move(storage)));
            binding->direct_slots.push_back(slot);++state.statistics.direct_slot_allocations;
        }
        auto reader=std::make_shared<DirectSlotReader>();reader->slot=slot;
        auto* mapped=static_cast<std::byte*>(slot->mImpl->buffer->value->mappedBytes());if(!mapped)return false;
        for(std::size_t region=0;region<regions.size();++region)
            std::memcpy(mapped+slot->regionOffset(static_cast<std::uint32_t>(region)),&regions[region],sizeof(regions[region]));
        std::memcpy(mapped+slot->globalsOffset(),&globals,sizeof(globals));
        slot->mImpl->reader=reader;view.direct_reader=std::move(reader);view.direct_normal_hdr=globals.flags[2];view.arena_command=origin;
        view.direct_command=const_cast<void*>(command.nativeIdentity());
        state.statistics.direct_view_bytes+=regions.size()*sizeof(ALMetalWorldSourceRegion)+sizeof(globals);return true;
    };
    // Exact weak control blocks follow the cheap hash. A warm composition
    // visits G owner/view records and cached physical pages, never N sources.
    // Expired unrelated compositions are handled by the explicit frame-owner
    // trim boundary or the cold path, not scanned per source/group here.
    for(auto entry=state.cohorts.begin();entry!=state.cohorts.end();++entry)
    {
        if(!state.current(*entry)||entry->hash!=composition_hash||entry->owners.size()!=selections.size()||
           bool(entry->binding->direct_policy)!=direct_bindings)continue;
        bool matches=true;
        for(std::size_t group=0;group<selections.size();++group)
        {
            const auto& proof=entry->owners[group];const auto& selected=selections[group];
            if(proof.cohort.lock()!=selected.cohort || proof.cohort.owner_before(selected.cohort)||selected.cohort.owner_before(proof.cohort) ||
               proof.cpu_pointer!=selected.cpu.get() || proof.cpu.owner_before(selected.cpu)||selected.cpu.owner_before(proof.cpu) ||
               proof.native_pointer!=selected.native.get() || proof.native.owner_before(selected.native)||selected.native.owner_before(proof.native))
            {matches=false;break;}
        }
        if(!matches)continue;
        auto view=std::make_unique<ALMetalWorldSourceFrameView::Impl>();view->context=state.context;
        view->cohort=entry->binding;view->mapping=entry->binding->mapping;view->ranges=std::move(ranges);
        view->native_owners.reserve(entry->binding->native_owner_indices.size());
        for(auto owner:entry->binding->native_owner_indices)view->native_owners.push_back(selections[owner].native);
        if(direct_bindings)
        {if(!publishDirect(*view,entry->binding))return reject("Native direct cohort slot publication failed");}
        else if(!view->publishView(&command,frame_data,regions,globals,error))return reject("Native cohort pass per-view publication failed");
        if(frame_data&&!direct_bindings){++state.statistics.arena_views;state.statistics.arena_view_bytes+=((regions.size()*sizeof(ALMetalWorldSourceRegion)+255)&~std::size_t(255))+sizeof(globals);}
        if(!direct_bindings)for(const auto& proof:entry->binding->pages)
        {
            auto& page=state.pages[proof.page];auto reader=page->reader.lock();
            if(!reader){reader=std::make_shared<PageReader>();reader->page=page;page->reader=reader;}
            view->pages.push_back(std::move(reader));
        }
        if(!direct_bindings)
        {
            view->bank=state.bank->reader.lock();
            if(!view->bank){view->bank=std::make_shared<BankReader>();view->bank->bank=state.bank;state.bank->reader=view->bank;}
        }
        state.cohorts.splice(state.cohorts.end(),state.cohorts,entry);
        ++state.statistics.cohort_hits;if(!frame_data&&!direct_bindings)state.statistics.view_bytes+=regions.size()*sizeof(ALMetalWorldSourceRegion)+sizeof(globals);
        return std::shared_ptr<const ALMetalWorldSourceFrameView>(new ALMetalWorldSourceFrameView(std::move(view)));
    }
    state.pruneCohorts();
    // The only N expansion is cold publication from completed private static
    // records. Descriptor-free direct sources remain unchanged.
    std::vector<ALMetalWorldSourceRequest> requests;requests.reserve(total);
    std::vector<std::shared_ptr<const ALMetalWorldResourceSource>> resources;resources.reserve(total);
    for(const auto& selected:selections)
    {
        std::unordered_map<const ALMetalTexture*,std::shared_ptr<const ALMetalTexture>> exact_textures;
        for(std::size_t index=0;index<selected.cohort->size();++index)
        {
            const auto& source=selected.cohort->source(index);auto descriptor=source.native_resource_source;
            if(!descriptor)
            {
                std::vector<std::shared_ptr<const ALMetalTexture>> leases;
                const auto add=[&](const ALMetalTexture* texture)
                {
                    if(!texture)return false;
                    auto found=exact_textures.find(texture);
                    if(found==exact_textures.end())
                    {
                        auto lease=selected.cohort->resolveNativeTexture(selected.native,texture);
                        if(!lease)return false;
                        found=exact_textures.emplace(texture,std::move(lease)).first;
                    }
                    if(std::none_of(leases.begin(),leases.end(),[&](const auto& lease){return lease.get()==texture;}))leases.push_back(found->second);
                    return true;
                };
                for(std::size_t slot=0;slot<source.texture_count;++slot)
                {
                    if(!add(source.diffuse_textures[slot]))return reject("Native cohort diffuse generation proof is unavailable");
                    if(source.pbr_slot_count)
                    {
                        const auto& material=source.pbrSlots()[slot];
                        if(!add(material.normal_texture)||!add(material.orm_texture)||!add(material.emissive_texture))
                            return reject("Native cohort PBR generation proof is unavailable");
                    }
                    else
                    {
                        const auto& material=source.materialSlots()[slot];
                        if((material.normal_texture&&!add(material.normal_texture))||(material.specular_texture&&!add(material.specular_texture)))
                            return reject("Native cohort Legacy generation proof is unavailable");
                    }
                }
                auto prototype=source;descriptor=ALMetalWorldResourceSource::publish(*state.context->device,prototype,leases,error);
                if(!descriptor)return reject("Native cohort optional resource certification failed");
            }
            resources.push_back(std::move(descriptor));ALMetalWorldSourceRequest request;
            request.key=selected.cohort->key(index);request.source=&source;request.region=selected.region;
            request.cpu_owner=selected.cpu;request.native.owner=selected.native;requests.push_back(std::move(request));
        }
    }
    const auto policy_bytes=direct_bindings?requests.size()*96:0;
    auto frozen=freezeInternal(&command,requests,regions,globals,error,resources,true,frame_data,policy_bytes);
    if(!frozen){++state.statistics.cohort_failures;return {};}
    auto membership=std::make_unique<ALMetalWorldSourceCommandMembership::Impl>();
    membership->device=(__bridge void*)state.context->native;membership->calls.reserve(requests.size());
    for(std::size_t index=0;index<requests.size();++index)
    {
        const auto& source=*requests[index].source;
        if(source.index_count>std::numeric_limits<std::uint32_t>::max())return reject("Native cohort indexed count overflows");
        membership->index_complete&=bool(source.indices->resourceView());
        appendPlanView(membership->index_views,source.indices->resourceView());
        membership->calls.push_back({requests[index].key,source.indices->nativeHandle(),source.index_offset,
            static_cast<std::uint32_t>(source.index_count),source.index_type,frozen->rowId(index),frozen->allocationGeneration(index)});
        if(membership->runs.empty()||membership->runs.back().double_sided!=source.double_sided)
            membership->runs.push_back({index,1,source.double_sided});
        else ++membership->runs.back().count;
    }
    auto binding=std::make_shared<CohortBinding>();
    if(direct_bindings)
    {
        binding->direct_policy=makePhysicalBuffer(state.context,policy_bytes,"Alchemy immutable direct UV and material policy",error);
        if(!binding->direct_policy)return reject("Native direct cohort policy publication failed");
        auto* mapped=static_cast<std::byte*>(binding->direct_policy->value->mappedBytes());if(!mapped)return reject("Native direct policy is not mapped");
        membership->bindings.reserve(requests.size());membership->policy=binding->direct_policy->value->nativeHandle();
        std::size_t bounds_group=0,bounds_first=0;
        for(std::size_t index=0;index<requests.size();++index)
        {
            const auto& signature=state.rows[frozen->rowId(index)].signature;
            ALMetalWorldSourceDirectBinding direct;
            std::copy_n(signature.handles.begin(),7,direct.geometry.begin());direct.offsets=signature.offsets;
            direct.legacy_numeric=signature.handles[7];direct.pbr_ldr=signature.handles[8];direct.pbr_hdr=signature.handles[9];
            direct.resource_table=signature.handles[family==2?11:10];direct.region=requests[index].region;direct.policy_offset=index*96;
            while(index-bounds_first>=selections[bounds_group].cohort->size())
            {bounds_first+=selections[bounds_group].cohort->size();++bounds_group;}
            const auto& bounds=selections[bounds_group].cohort->bounds(index-bounds_first);
            std::copy_n(bounds.minimum,4,direct.bounds_minimum.begin());
            std::copy_n(bounds.maximum,4,direct.bounds_maximum.begin());
            direct.bounds_qualified=bounds.qualified;
            std::memcpy(mapped+direct.policy_offset,signature.values.data(),96);membership->bindings.push_back(direct);
        }
        state.statistics.direct_policy_bytes+=policy_bytes;
    }
    binding->membership=std::shared_ptr<const ALMetalWorldSourceCommandMembership>(new ALMetalWorldSourceCommandMembership(std::move(membership)));
    std::set<std::shared_ptr<const void>,std::owner_less<std::shared_ptr<const void>>> unique_native_owners;
    for(std::size_t owner=0;owner<selections.size();++owner)
        if(unique_native_owners.insert(selections[owner].native).second)binding->native_owner_indices.push_back(static_cast<std::uint32_t>(owner));
    auto& view=*frozen->mImpl;binding->mapping=view.mapping;binding->tables=std::move(view.tables);binding->geometry=std::move(view.geometry);
    binding->plan_views=std::move(view.plan_views);binding->plan_complete=view.plan_complete;
    binding->declaration=state.context->resources->prepareDeclaration(binding->tables,
        state.limits.maximum_metadata_bytes,error);
    if(!binding->declaration)return reject("Native cohort resource declaration publication failed");
    const auto declaration_bytes=binding->declaration->metadataBytes()+sizeof(DeclarationMetadata)+2*sizeof(void*);
    const auto metadata_used=state.context->budget->bytes.load(std::memory_order_relaxed);
    if(metadata_used>state.context->budget->limit||declaration_bytes>state.context->budget->limit-metadata_used)
        return reject("Native cohort resource declaration metadata is full");
    auto declaration_metadata=std::make_shared<DeclarationMetadata>();declaration_metadata->budget=state.context->budget;
    declaration_metadata->bytes=declaration_bytes;
    state.context->budget->bytes.fetch_add(declaration_bytes,std::memory_order_relaxed);
    binding->declaration_metadata=std::move(declaration_metadata);
    binding->pages.reserve(view.pages.size());
    for(std::size_t page=0;page<state.pages.size();++page)
        if(std::any_of(view.pages.begin(),view.pages.end(),[&](const auto& reader){return reader->page==state.pages[page];}))
            binding->pages.push_back({static_cast<std::uint32_t>(page)});
    std::unordered_set<void*> numeric;
    for(const auto& buffer:view.buffers)if(numeric.insert(buffer->nativeHandle()).second)binding->numeric.push_back(buffer->nativeHandle());
    // Native argument tables retain texture views independently of the C++
    // tuple. Charge unique root allocations, not wrapper size or view extent.
    std::unordered_set<void*> allocations;
    for(const auto& texture:view.textures)
    {
        id<MTLTexture> root=(__bridge id<MTLTexture>)texture->nativeHandle();unsigned depth=0;
        while(root.parentTexture && depth++<32)root=root.parentTexture;
        if(!root || root.parentTexture || root.buffer || !root.allocatedSize)
        {binding->asset_footprint_known=false;break;}
        void* identity=(__bridge void*)root;
        if(allocations.insert(identity).second)binding->assets.push_back({identity,root.allocatedSize});
    }
    // PPD425: captured graph Views keep exact native geometry/numeric
    // allocations alive in the idle cache. Include them in this same gross
    // cached-asset scope; aliases and repeated rows count one root allocation.
    const auto append_buffer_asset=[&](const ALMetalBuffer& buffer)
    {
        id<MTLBuffer> native=(__bridge id<MTLBuffer>)buffer.nativeHandle();
        if(!native||native.device!=state.context->native||!native.allocatedSize)
        {binding->asset_footprint_known=false;return;}
        void* root=(__bridge void*)native;
        if(allocations.insert(root).second)binding->assets.push_back({root,native.allocatedSize});
    };
    for(const auto& request:requests)
    {append_buffer_asset(*request.source->vertices);append_buffer_asset(*request.source->indices);}
    for(const auto& buffer:view.buffers)append_buffer_asset(*buffer);
    if(direct_bindings)
    {
        auto& direct=*binding->membership->mImpl;
        if(!binding->asset_footprint_known)return reject("Native direct cohort asset footprint is unknown");
        for(const auto& asset:binding->assets)direct.assets.push_back({asset.allocation,asset.bytes});
        if(!publishDirect(view,binding))return reject("Native direct cohort slot publication failed");
        view.arena_buffer=nil;view.regions.reset();view.globals.reset();view.pages.clear();view.bank.reset();
    }
    binding->metadata=sizeof(CohortBinding)+sizeof(ALMetalWorldSourceCommandMembership)+sizeof(ALMetalWorldSourceCommandMembership::Impl)+
        binding->membership->mImpl->index_views.capacity()*sizeof(RP::View)+binding->plan_views.capacity()*sizeof(RP::View)+
        binding->membership->mImpl->calls.capacity()*sizeof(ALMetalWorldSourceIndexedCall)+binding->membership->mImpl->runs.capacity()*sizeof(ALMetalWorldSourceCommandRun)+
        binding->pages.capacity()*sizeof(CohortBinding::PageProof)+(binding->geometry.capacity()+binding->numeric.capacity())*sizeof(void*)+
        binding->tables.capacity()*sizeof(binding->tables[0])+sizeof(Impl::CohortEntry)+2*sizeof(void*)+
        selections.size()*sizeof(Impl::CohortEntry::Owner)+binding->native_owner_indices.capacity()*sizeof(std::uint32_t)+
        binding->membership->calls().size()*sizeof(CohortRowWatch)+
        binding->assets.capacity()*sizeof(CohortBinding::Asset)+binding->assets.size()*(sizeof(void*)+sizeof(Impl::AssetRef)+2*sizeof(void*));
    binding->metadata+=binding->membership->mImpl->bindings.capacity()*sizeof(ALMetalWorldSourceDirectBinding)+
        binding->membership->mImpl->assets.capacity()*sizeof(ALMetalWorldSourceNativeFootprint)+
        binding->declaration_metadata->bytes;
    // Actual command views retain only the completed native tuple and native
    // metadata. Private CPU certificates/prototypes can die after encode.
    view.cohort=binding;view.ranges=std::move(ranges);
    decltype(view.buffers){}.swap(view.buffers);
    decltype(view.textures){}.swap(view.textures);decltype(view.rows){}.swap(view.rows);
    decltype(view.generations){}.swap(view.generations);decltype(view.commands){}.swap(view.commands);
    std::uint64_t own_asset_bytes=0;
    for(const auto& asset:binding->assets)
    {
        if(asset.bytes>std::numeric_limits<std::uint64_t>::max()-own_asset_bytes)
        {binding->asset_footprint_known=false;break;}
        own_asset_bytes+=asset.bytes;
    }
    if(binding->asset_footprint_known && own_asset_bytes<=state.limits.maximum_cached_cohort_native_asset_bytes &&
       state.limits.maximum_cached_cohorts && requests.size()<=state.limits.maximum_cached_cohort_rows &&
       binding->metadata<=state.limits.maximum_cached_cohort_metadata_bytes)
    {
        std::list<Impl::CohortEntry> staged;
        const auto rollback=[&]()noexcept
        {
            for(const auto& asset:binding->assets)
                if(auto found=state.cohort_assets.find(asset.allocation);found!=state.cohort_assets.end())
                {if(!found->second.references)state.cohort_assets.erase(found);else found->second.staged=false;}
        };
        std::uint64_t prospective=state.cohort_asset_bytes;
        try
        {
            // Complete every fallible cache-node/accounting allocation before
            // evicting anything. Optional cache OOM preserves this valid view
            // and all older usable warm entries (PPD209).
            staged.emplace_back();auto& candidate=staged.back();candidate.hash=composition_hash;candidate.binding=binding;
            candidate.owners.reserve(selections.size());
            for(const auto& selection:selections)
                candidate.owners.push_back({selection.cohort,selection.cpu,selection.native,selection.cpu.get(),selection.native.get()});
            // Account the actual retained vector capacity, including an
            // implementation that reserves more than the requested G slots.
            binding->metadata+=(candidate.owners.capacity()-selections.size())*sizeof(Impl::CohortEntry::Owner);
            if(binding->metadata>state.limits.maximum_cached_cohort_metadata_bytes)
            {++state.statistics.cohort_builds;return frozen;}
            candidate.watches=std::make_unique<CohortRowWatch[]>(binding->membership->calls().size());
            // A partially staged array remains unregistered on failure.
            if(state.force_cache_failure){state.force_cache_failure=false;throw std::bad_alloc();}
            state.cohort_assets.reserve(state.cohort_assets.size()+binding->assets.size());
            for(const auto& asset:binding->assets)
            {
                auto found=state.cohort_assets.try_emplace(asset.allocation,Impl::AssetRef{asset.bytes}).first;
                if(found->second.bytes!=asset.bytes){rollback();++state.statistics.cohort_builds;return frozen;}
                found->second.staged=true;
                if(!found->second.references)
                {
                    if(asset.bytes>std::numeric_limits<std::uint64_t>::max()-prospective)
                    {rollback();++state.statistics.cohort_builds;return frozen;}
                    prospective+=asset.bytes;
                }
            }
        }
        catch(const std::bad_alloc&){rollback();++state.statistics.cohort_builds;return frozen;}
        // From here commit is allocation-free. Tables/pages already published
        // under command admission; cache-only assets use their separate cap.
        while(!state.cohorts.empty() && (state.cohorts.size()>=state.limits.maximum_cached_cohorts ||
              state.cohort_rows>state.limits.maximum_cached_cohort_rows-requests.size() ||
              state.cohort_metadata>state.limits.maximum_cached_cohort_metadata_bytes-binding->metadata ||
              prospective>state.limits.maximum_cached_cohort_native_asset_bytes))
            prospective-=state.eraseCohort(state.cohorts.begin());
        for(const auto& asset:binding->assets)
        {
            auto& ref=state.cohort_assets.find(asset.allocation)->second;
            if(!ref.references++)state.cohort_asset_bytes+=ref.bytes;
            ref.staged=false;
        }
        auto& incoming=staged.back();const auto calls=binding->membership->calls();
        for(std::size_t index=0;index<calls.size();++index)
        {
            auto& head=state.rows[calls[index].row_id].watchers;auto& watch=incoming.watches[index];
            watch.head=&head;watch.entry=&incoming.validity;watch.next=head;
            if(head)head->previous=&watch;
            head=&watch;
        }
        state.cohorts.splice(state.cohorts.end(),staged);
        state.cohort_rows+=requests.size();state.cohort_metadata+=binding->metadata;state.cohort_references+=selections.size();
    }
    ++state.statistics.cohort_builds;return frozen;
}
catch(const std::bad_alloc&){++mImpl->statistics.cohort_failures;if(error)error->clear();return {};}

void ALMetalWorldSourcePages::setPublicationAdmission(PublicationAdmission admission)
{
    auto holder=admission?std::make_shared<PublicationAdmission>(std::move(admission)):nullptr;
    mImpl->admission=std::move(holder);++mImpl->epoch;
}
std::shared_ptr<const ALMetalWorldSourceFrameView> ALMetalWorldSourcePages::freezeInternal(
    ALMetalCommandBuffer* command,std::span<const ALMetalWorldSourceRequest> requests,
    std::span<const ALMetalWorldSourceRegion> regions,const ALMetalWorldSourceGlobals& globals,std::string* error,
    std::span<const std::shared_ptr<const ALMetalWorldResourceSource>> overrides,bool certified_cohort,ALMetalWorldFrameData* frame_data,
    std::size_t direct_policy_bytes)
try
{
    auto& state=*mImpl;
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldSourceFrameView>
    {++state.statistics.failures;fail(error,message);return {};};
    const auto commandValid=[&]{return !command||(!command->isCommitted()&&command->nativeDevice()==(__bridge void*)state.context->native);};
    if((frame_data&&!certified_cohort)||(!overrides.empty()&&overrides.size()!=requests.size())||!state.supported||requests.empty()||requests.size()>alMetalWorldSourceMaximumRows||regions.empty()||
       regions.size()>alMetalWorldSourceMaximumRows||!finite(globals.mirror_clip_plane,4)||
       globals.flags[0]>1||globals.flags[1]>1||globals.flags[2]>1||globals.flags[3]||!commandValid())
        return reject("Native source view shape or command is invalid");
    for(const auto& region:regions)
        if(!finite(region.modelview_projection,16)||!finite(region.modelview,16)||region.modelview[15]!=1||
           !finite(&region.normal_matrix[0][0],12))return reject("Native source regional matrices are invalid");
    if(state.force_failure){state.force_failure=false;return reject("Forced native source publication failure");}
    struct PlannedRow
    {
        ALMetalWorldSourceRequest request;
        SourceInput input;
        Signature signature;
        std::uint32_t slot=0,generation=0;
        std::size_t table=0;
        bool allocate=false,hit=false;
    };
    std::vector<PlannedRow> rows;
    std::vector<std::size_t> request_rows;
    std::unordered_map<Key,std::size_t,KeyHash> selected;
    std::vector<std::uint32_t> mapping(state.limits.maximum_rows,std::numeric_limits<std::uint32_t>::max());
    auto free=state.free;
    std::vector<bool> evicted(state.rows.size(),false);
    auto victim=state.lru.begin();
    std::vector<std::shared_ptr<const ALMetalWorldResourcePreparation>> table_plans;
    std::unordered_map<const ALMetalWorldResourceSource*,std::size_t> source_tables;
    std::unordered_map<std::size_t,std::vector<std::size_t>> table_hashes;
    std::array<bool,alMetalWorldSourceMaximumRows/alMetalWorldSourceRowsPerPage> dirty_pages{},used_pages{};
    std::array<std::shared_ptr<PageReader>,alMetalWorldSourceMaximumRows/alMetalWorldSourceRowsPerPage> old_readers{};
    std::shared_ptr<BankReader> old_bank_reader;
    ALMetalWorldSourcePublicationWork work;
    work.encoded_row_bytes=direct_policy_bytes;
    if(direct_policy_bytes)work.allocation_bytes=(direct_policy_bytes+4095)&~std::size_t(4095);
    rows.reserve(requests.size());request_rows.reserve(requests.size());selected.reserve(requests.size());
    const auto epoch=state.epoch;
    std::uint32_t highest_slot=0;
    std::unordered_set<void*> proven_buffers;
    proven_buffers.reserve(std::min(requests.size(),state.limits.maximum_rows)*5);
    // Read-only plan: no row/LRU/cache mutation, GPU allocation or mapped write.
    for(std::size_t request_index=0;request_index<requests.size();++request_index)
    {
        const auto& request=requests[request_index];
        ++state.statistics.requests;
        if(!request.key.identity||!request.key.revision||request.region>=regions.size())return reject("Native source request is incomplete");
        SourceInput input;if(!captureInput(request,regions[request.region],globals,input,error,overrides.empty()?nullptr:overrides[request_index],certified_cohort)){++state.statistics.failures;return {};}
        // Check the entire unique geometry/index/numeric union before any
        // resource-table allocation or argument-buffer write. Resource-source
        // certification proves textures, not these independent buffers.
        for(const ALMetalBuffer* buffer:{input.vertices,input.indices,input.numeric[0].get(),input.numeric[1].get(),input.numeric[2].get()})
            if(buffer && proven_buffers.insert(buffer->nativeHandle()).second && !state.proveDevice(*buffer,error))
            {++state.statistics.failures;return {};}
        const Key key{request.key.identity,request.key.flavor};
        if(auto existing=selected.find(key);existing!=selected.end())
        {
            const auto& prior=rows[existing->second];
            if(prior.request.key.revision!=request.key.revision||prior.request.region!=request.region||!sameInput(prior.input,input))
                return reject("Native source row has conflicting immutable inputs");
            request_rows.push_back(existing->second);continue;
        }
        std::size_t table_index=0;
        if(auto prior=source_tables.find(input.resources.get());prior!=source_tables.end())table_index=prior->second;
        else
        {
            auto preparation=state.context->resources->preflight(input.resources,*state.context->sampler,error);
            if(!preparation){++state.statistics.failures;return {};}
            auto& candidates=table_hashes[preparation->publicationHash()];
            const auto same=std::find_if(candidates.begin(),candidates.end(),[&](auto index)
                {return preparation->samePublication(*table_plans[index]);});
            if(same!=candidates.end())table_index=*same;
            else
            {
                table_index=table_plans.size();candidates.push_back(table_index);table_plans.push_back(std::move(preparation));
                work.encoded_table_bytes+=table_plans.back()->publicationBytes();
                work.allocation_bytes+=table_plans.back()->publicationBytes();
            }
            source_tables.emplace(input.resources.get(),table_index);
        }
        auto signature=input.signature;
        auto table=table_plans[table_index]->existingTable();
        if(table)signature.handles[input.pbr?11:10]=table->argumentLease()->nativeHandle();
        auto found=state.lookup.find(key);std::uint32_t slot=0;bool allocate=found==state.lookup.end()||evicted[found->second];
        if(!allocate)slot=found->second;
        else
        {
            if(!free.empty()){slot=free.back();free.pop_back();}
            else
            {
                while(victim!=state.lru.end()&&(mapping[*victim]!=std::numeric_limits<std::uint32_t>::max()||
                      evicted[*victim]||state.rows[*victim].generation==std::numeric_limits<std::uint32_t>::max()))++victim;
                if(victim==state.lru.end())return reject("Native source row capacity is exhausted");
                slot=*victim++;evicted[slot]=true;
            }
        }
        const auto& current=state.rows[slot];
        if(!allocate&&(request.key.revision<current.revision||request.key.revision<current.minimum_revision))
            return reject("Native source row publication is stale");
        if(allocate&&current.generation==std::numeric_limits<std::uint32_t>::max())return reject("Native source allocation generation is exhausted");
        const bool same_table=table&&!current.table.owner_before(table)&&!table.owner_before(current.table);
        const bool hit=!allocate&&current.valid&&current.revision==request.key.revision&&current.signature==signature&&
            !current.material.expired()&&(certified_cohort||!current.resources.expired())&&same_table;
        const auto index=rows.size();selected.emplace(key,index);request_rows.push_back(index);
        rows.push_back({request,std::move(input),signature,slot,current.generation+(allocate?1u:0u),table_index,allocate,hit});
        mapping[slot]=request.region;highest_slot=std::max(highest_slot,slot);used_pages[slot/alMetalWorldSourceRowsPerPage]=true;
        if(!hit)
        {
            dirty_pages[slot/alMetalWorldSourceRowsPerPage]=true;
            work.encoded_row_bytes+=state.context->row_encoder.encodedLength;
        }
    }
    if(!state.context->resources->validatePreparations(table_plans,error)){++state.statistics.failures;return {};}
    bool bank_change=false;
    for(std::size_t page=0;page<dirty_pages.size();++page)if(dirty_pages[page])
    {
        const auto& current=state.pages[page];
        if(current)old_readers[page]=current->reader.lock();
        const auto length=state.context->stride*alMetalWorldSourceRowsPerPage;
        if(!current||old_readers[page])
        {
            bank_change=true;work.allocation_bytes+=length;
            if(current)work.copied_page_bytes+=length;else work.zeroed_page_bytes+=length;
        }
    }
    if(bank_change)
    {
        work.encoded_bank_range_bytes=state.context->bank_length;
        if(state.bank)old_bank_reader=state.bank->reader.lock();
        if(!state.bank||old_bank_reader)
        {
            work.allocation_bytes+=state.context->bank_length;
            if(state.bank)work.copied_bank_bytes=state.context->bank_length;else work.zeroed_bank_bytes=state.context->bank_length;
        }
    }
    mapping.resize(std::size_t(highest_slot)+1);
    const bool zero_map=certified_cohort&&regions.size()==1;
    const auto mapping_bytes=zero_map?(state.cohort_mapping?0:state.limits.maximum_rows*sizeof(std::uint32_t)):mapping.size()*sizeof(std::uint32_t);
    // A cold certified composition retains its mapping in the native cache.
    // Charge that publication even when every row/table already matches. The
    // generic ephemeral map and required per-view camera data stay excluded.
    if(certified_cohort && mapping_bytes)
    {work.encoded_mapping_bytes=mapping_bytes;work.allocation_bytes+=mapping_bytes;}
    const auto view_bytes=(frame_data||direct_policy_bytes?0:regions.size_bytes()+sizeof(globals))+mapping_bytes;
    // Mapping capacity is already included exactly once in view_bytes.
    const auto page_allocation=work.allocation_bytes-work.encoded_table_bytes-work.encoded_mapping_bytes;
    const auto live=state.context->budget->bytes.load(std::memory_order_relaxed);
    if(live>state.context->budget->limit||page_allocation>state.context->budget->limit-live||
       view_bytes>state.context->budget->limit-live-page_allocation)return reject("Native source metadata capacity is exhausted");
    const auto charged=work.chargedBytes();
    if(charged==std::numeric_limits<std::uint64_t>::max())return reject("Native source publication work overflows");
    auto admission=state.admission;
    std::shared_ptr<const void> credit;
    if(charged&&admission)
    {
        if(!command)return reject("Admitted native source publication requires an actual command");
        const std::vector<ALMetalWorldSourceRequest> originals(requests.begin(),requests.end());
        const std::vector<ALMetalWorldSourceRegion> original_regions(regions.begin(),regions.end());
        const auto original_globals=globals;
        ++state.statistics.admission_requests;credit=(*admission)(work);
        if(!credit){++state.statistics.admission_denials;return reject("Native source publication admission was deferred");}
        if(state.epoch!=epoch||!commandValid())return reject("Native source publication plan was retired during admission");
        if(std::memcmp(original_regions.data(),regions.data(),regions.size_bytes())||std::memcmp(&original_globals,&globals,sizeof(globals)))
            return reject("Native source view changed during admission");
        // The policy is an owner callback. Validate captured source facts after
        // it returns, without trusting a reentrant/mutated borrowed prototype.
        for(std::size_t index=0;index<requests.size();++index)
        {
            const auto& original=originals[index];const auto& current=requests[index];const auto& row=rows[request_rows[index]];
            if(current.key.identity!=original.key.identity||current.key.flavor!=original.key.flavor||current.key.revision!=original.key.revision||
               current.source!=original.source||current.region!=original.region||current.native.vertices!=original.native.vertices||
               current.native.indices!=original.native.indices||current.native.owner.owner_before(original.native.owner)||
               original.native.owner.owner_before(current.native.owner))return reject("Native source request changed during admission");
            SourceInput now;
            if(!captureInput(current,regions[current.region],globals,now,error,overrides.empty()?nullptr:overrides[index],certified_cohort)||!sameInput(now,row.input))
                return reject("Native source input changed during admission");
        }
        if(!state.context->resources->validatePreparations(table_plans,error)){++state.statistics.failures;return {};}
        if(!command->retainUntilCompletion(credit,error)){++state.statistics.failures;return {};}
        state.statistics.admitted_bytes+=charged;
    }
    if(state.epoch!=epoch)return reject("Native source publication plan is stale");
    // All optional admission and read-only proofs have completed. Credits are
    // now command-owned, never attached to the idle publisher or FrameView.
    ++state.epoch;if(++state.serial==0)++state.serial;
    for(const auto& planned:rows)if(planned.allocate)
    {
        auto& row=state.rows[planned.slot];
        if(!row.allocated)
        {
            if(state.free.empty()||state.free.back()!=planned.slot)return reject("Native source free-slot plan is stale");
            state.free.pop_back();
        }
        if(row.allocated)
        {
            const auto old=state.lookup.find(row.key);if(old!=state.lookup.end()&&old->second==planned.slot)state.lookup.erase(old);
            const auto identity=state.identities.find(row.key.identity);
            if(identity!=state.identities.end())
            {std::erase(identity->second,planned.slot);if(identity->second.empty())state.identities.erase(identity);}
            if(auto source=row.resources.lock())state.context->resources->releaseSourceCache(source);
        }
        state.changed(planned.slot);
        row.key={planned.request.key.identity,planned.request.key.flavor};row.generation=planned.generation;
        row.valid=false;row.allocated=true;row.revision=row.minimum_revision=0;row.material.reset();row.resources.reset();row.table.reset();
        state.identities[row.key.identity].push_back(planned.slot);state.lookup.emplace(row.key,planned.slot);
    }
    std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> prepared_tables;prepared_tables.reserve(table_plans.size());
    for(const auto& plan:table_plans)
    {auto table=state.context->resources->prepare(*plan,error);if(!table){++state.statistics.failures;return {};}prepared_tables.push_back(std::move(table));}
    for(auto& planned:rows)
    {
        auto& row=state.rows[planned.slot];
        planned.signature.handles[planned.input.pbr?11:10]=prepared_tables[planned.table]->argumentLease()->nativeHandle();
        if(!planned.hit)
        {
            row.valid=false;state.changed(planned.slot);
            if(!state.preparePage(planned.slot/alMetalWorldSourceRowsPerPage,error)){++state.statistics.failures;return {};}
            auto& page=state.pages[planned.slot/alMetalWorldSourceRowsPerPage];const auto& signature=planned.signature;
            [state.context->row_encoder setArgumentBuffer:(__bridge id<MTLBuffer>)page->buffer->value->nativeHandle()
                offset:(planned.slot%alMetalWorldSourceRowsPerPage)*state.context->stride];
            for(unsigned binding=0;binding<7;++binding)
                [state.context->row_encoder setBuffer:(__bridge id<MTLBuffer>)signature.handles[binding] offset:signature.offsets[binding] atIndex:binding];
            for(unsigned binding=7;binding<12;++binding)
                [state.context->row_encoder setBuffer:(__bridge id<MTLBuffer>)signature.handles[binding] offset:0 atIndex:binding];
            std::memcpy([state.context->row_encoder constantDataAtIndex:12],signature.values.data(),64);
            std::memcpy([state.context->row_encoder constantDataAtIndex:13],signature.values.data()+16,16);
            std::memcpy([state.context->row_encoder constantDataAtIndex:14],signature.values.data()+20,16);
            [state.context->row_encoder setArgumentBuffer:nil offset:0];
            row.signature=signature;row.revision=planned.request.key.revision;row.material=planned.input.material;
            row.resources=planned.input.resources;row.table=prepared_tables[planned.table];row.valid=true;
            ++state.statistics.row_publications;state.statistics.encoded_row_bytes+=state.context->row_encoder.encodedLength;
        }
        else ++state.statistics.hits;
        row.last_use=state.serial;state.lru.splice(state.lru.end(),state.lru,row.lru);
    }
    auto view=std::make_unique<ALMetalWorldSourceFrameView::Impl>();view->context=state.context;view->tables=std::move(prepared_tables);
    if(!direct_policy_bytes && !view->publishView(command,frame_data,regions,globals,error)){++state.statistics.failures;return {};}
    if(frame_data&&!direct_policy_bytes){++state.statistics.arena_views;state.statistics.arena_view_bytes+=((regions.size_bytes()+255)&~std::size_t(255))+sizeof(globals);}
    if(zero_map)
    {
        if(!state.cohort_mapping)
        {
            auto map=makeBuffer(state.context,state.limits.maximum_rows*sizeof(std::uint32_t),"Alchemy immutable cohort region-zero map",error);
            if(!map){++state.statistics.failures;return {};}
            std::memset(map->value->mappedBytes(),0,map->value->length());state.cohort_mapping=std::move(map);
        }
        view->mapping=state.cohort_mapping;
    }
    else view->mapping=makeBuffer(state.context,mapping.size()*sizeof(std::uint32_t),"Alchemy source to view region map",error);
    if(!view->mapping){++state.statistics.failures;return {};}
    if(!zero_map)std::memcpy(view->mapping->value->mappedBytes(),mapping.data(),mapping.size()*sizeof(std::uint32_t));
    state.statistics.encoded_mapping_bytes+=work.encoded_mapping_bytes;
    std::set<std::shared_ptr<const void>,std::owner_less<std::shared_ptr<const void>>> owners;
    std::unordered_set<const ALMetalBuffer*> buffers;std::unordered_set<const ALMetalTexture*> textures;std::unordered_set<void*> geometry;
    view->rows.reserve(requests.size());view->generations.reserve(requests.size());view->commands.reserve(requests.size());
    for(std::size_t index=0;index<requests.size();++index)
    {
        const auto& planned=rows[request_rows[index]];const auto& request=planned.request;
        view->rows.push_back(planned.slot);view->generations.push_back(planned.generation);
        view->commands.push_back({request.key,planned.input.indices,planned.input.indices->nativeHandle()});
        if(owners.insert(request.native.owner).second)view->native_owners.push_back(request.native.owner);
        for(const auto* buffer:{planned.input.vertices,planned.input.indices})
        {view->plan_complete&=bool(buffer->resourceView());appendPlanView(view->plan_views,buffer->resourceView());}
        for(const auto& buffer:planned.input.numeric)if(buffer)
        {view->plan_complete&=bool(buffer->resourceView());appendPlanView(view->plan_views,buffer->resourceView());}
        for(auto handle:{planned.input.vertices->nativeHandle(),planned.input.indices->nativeHandle()})
            if(geometry.insert(handle).second)view->geometry.push_back(handle);
        for(const auto& buffer:{request.native.vertices,request.native.indices})if(buffer&&buffers.insert(buffer.get()).second)view->buffers.push_back(buffer);
        for(const auto& buffer:planned.input.numeric)if(buffer&&buffers.insert(buffer.get()).second)view->buffers.push_back(buffer);
        for(const auto& texture:planned.input.resources->generationLeases())if(texture&&textures.insert(texture.get()).second)view->textures.push_back(texture);
    }
    for(std::size_t page=0;page<used_pages.size();++page)if(used_pages[page])
    {
        auto reader=state.pages[page]->reader.lock();
        if(!reader){reader=std::make_shared<PageReader>();reader->page=state.pages[page];state.pages[page]->reader=reader;}
        view->pages.push_back(std::move(reader));
    }
    view->bank=state.bank->reader.lock();
    if(!view->bank){view->bank=std::make_shared<BankReader>();view->bank->bank=state.bank;state.bank->reader=view->bank;}
    state.pruneCohorts();
    state.statistics.view_bytes+=view_bytes;
    return std::shared_ptr<const ALMetalWorldSourceFrameView>(new ALMetalWorldSourceFrameView(std::move(view)));
}
catch(const std::bad_alloc&){++mImpl->statistics.failures;if(error)error->clear();return {};}
bool ALMetalWorldSourcePages::prepareAnimatedSourcesForCommand(ALMetalCommandBuffer& command,
    std::span<const ALMetalWorldSourceReference* const> references,
    std::vector<ALMetalWorldAnimatedGpuView>& output,std::string* error)
try
{
    output.clear();auto& state=*mImpl;
    if(state.animated_preparing)return false;
    struct Guard{bool& active;~Guard(){active=false;}}guard{state.animated_preparing};state.animated_preparing=true;
    if(!state.supported || references.empty() || references.size()>state.limits.maximum_rows ||
       command.isCommitted() || command.nativeDevice()!=state.context->device->nativeHandle())return false;
    state.pruneAnimated();const auto epoch=state.epoch;
    struct Task
    {
        std::shared_ptr<const ALMetalWorldAnimatedSource> source;
        std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
        std::shared_ptr<const void> native;
        ALMetalWorldViewMatrices view;
        std::shared_ptr<const ALMetalWorldAnimatedGpuSource> gpu;
        std::shared_ptr<const ALMetalWorldResourcePreparation> table;
        std::vector<ALMetalWorldSourceNativeFootprint> assets;
    };
    std::vector<Task> tasks;tasks.reserve(references.size());
    ALMetalWorldSourcePublicationWork work;
    std::unordered_set<const ALMetalWorldAnimatedSource*> cold;
    std::vector<std::shared_ptr<const ALMetalWorldResourcePreparation>> table_plans;
    std::unordered_map<void*,std::uint64_t> new_assets;
    for(const auto* reference:references)
    {
        if(!reference || !reference->animated_source || !reference->animated_source->gpuReady() || !reference->animated_palette ||
           reference->source!=&reference->animated_source->prototype() ||
           !ALMetalWorldSourceCohort::validView(reference->view) ||
           std::memcmp(reference->view.texture_matrix,reference->source->texture_matrix,64))return false;
        Task task;task.source=reference->animated_source->owner();task.native=reference->animated_source->nativeLease();
        task.pose=reference->animated_palette->shared_from_this();task.view=reference->view;
        if(!task.source || !task.native || !task.pose)return false;
        if(auto found=state.animated.find(task.source.get());found!=state.animated.end())
        {
            const auto& entry=found->second;
            if(!entry.source.owner_before(task.source) && !task.source.owner_before(entry.source) &&
               !entry.native.owner_before(task.native) && !task.native.owner_before(entry.native))task.gpu=entry.gpu;
        }
        if(!task.gpu && cold.insert(task.source.get()).second)
        {
            const auto& batch=task.source->prototype();
            const auto& material=*batch.native_material_source;
            const std::array<const ALMetalBuffer*,5> buffers{batch.vertices,batch.indices,
                material.materialLease().get(),material.pbrLease(false).get(),material.pbrLease(true).get()};
            for(const auto* buffer:buffers)if(buffer && !state.proveDevice(*buffer,error))return false;
            task.table=state.context->resources->preflight(batch.native_resource_source,*state.context->sampler,error);
            if(!task.table)return false;
            if(std::none_of(table_plans.begin(),table_plans.end(),[&](const auto& prior){return prior->samePublication(*task.table);}))
            {work.encoded_table_bytes+=task.table->publicationBytes();table_plans.push_back(task.table);}
            const auto bytes=96+std::max<std::size_t>(1,task.source->boneBounds().size())*48;
            work.encoded_row_bytes+=bytes;work.allocation_bytes+=(bytes+4095)&~std::uint64_t(4095);
            std::unordered_set<void*> seen;
            for(const auto* buffer:buffers)if(buffer)
            {
                id<MTLBuffer> native=(__bridge id<MTLBuffer>)buffer->nativeHandle();
                if(!native||native.device!=state.context->native||!native.allocatedSize)return false;
                void* allocation=(__bridge void*)native;
                if(seen.insert(allocation).second)task.assets.push_back({allocation,native.allocatedSize});
                if(!state.cohort_assets.contains(allocation))new_assets.emplace(allocation,native.allocatedSize);
            }
            for(const auto& texture:batch.native_resource_source->generationLeases())
            {
                id<MTLTexture> root=(__bridge id<MTLTexture>)texture->nativeHandle();unsigned depth=0;
                while(root.parentTexture && depth++<32)root=root.parentTexture;
                if(!root || root.parentTexture || root.buffer || !root.allocatedSize)return false;
                void* native=(__bridge void*)root;
                if(seen.insert(native).second)task.assets.push_back({native,root.allocatedSize});
                if(!state.cohort_assets.contains(native))new_assets.emplace(native,root.allocatedSize);
            }
        }
        tasks.push_back(std::move(task));
    }
    if(state.lookup.size()+state.animated.size()+cold.size()>state.limits.maximum_rows)return false;
    std::uint64_t asset_bytes=state.cohort_asset_bytes;
    for(const auto& [native,bytes]:new_assets)
    {if(bytes>state.limits.maximum_cached_cohort_native_asset_bytes-std::min(asset_bytes,state.limits.maximum_cached_cohort_native_asset_bytes))return false;asset_bytes+=bytes;}
    std::shared_ptr<const void> credit;
    if(work.chargedBytes() && state.admission)
    {
        ++state.statistics.admission_requests;auto admission=state.admission;credit=(*admission)(work);
        if(!credit){++state.statistics.admission_denials;return false;}
        if(epoch!=state.epoch)return false;
        if(!command.retainUntilCompletion(credit,error))return false;
        state.statistics.admitted_bytes+=work.chargedBytes();
    }
    if(epoch!=state.epoch || !state.context->resources->validatePreparations(table_plans,error))return false;
    if(state.force_failure){state.force_failure=false;return false;}
    state.animated.reserve(state.animated.size()+cold.size());state.cohort_assets.reserve(state.cohort_assets.size()+new_assets.size());
    output.reserve(tasks.size());
    for(auto& task:tasks)
    {
        if(!task.gpu)
        {
            if(auto found=state.animated.find(task.source.get());found!=state.animated.end())task.gpu=found->second.gpu;
            else
            {
                const auto& batch=task.source->prototype();auto table=state.context->resources->prepare(*task.table,error);if(!table)return false;
                const auto bones=task.source->boneBounds();const auto bytes=96+std::max<std::size_t>(1,bones.size())*48;
                auto buffer=makePhysicalBuffer(state.context,bytes,"Alchemy immutable animated source metadata",error);if(!buffer)return false;
                auto* mapped=static_cast<std::byte*>(buffer->value->mappedBytes());if(!mapped)return false;
                std::memcpy(mapped,batch.texture_matrix,64);
                const float material[]{batch.alpha_cutoff,batch.use_vertex_color?1.f:0.f,batch.alpha_cutoff_bias?1.f:0.f,batch.alpha_mask?1.f:0.f};
                const std::uint32_t policy[]{batch.texture_count,0,0,0};std::memcpy(mapped+64,material,16);std::memcpy(mapped+80,policy,16);
                if(bones.empty())std::memset(mapped+96,0,48);else std::memcpy(mapped+96,bones.data(),bones.size_bytes());
                auto gpu=std::make_unique<ALMetalWorldAnimatedGpuSource::Impl>();gpu->source=task.source;gpu->native=task.native;gpu->native_pointer=task.native.get();gpu->resources=state.context->resources;gpu->policy=buffer;gpu->bones=buffer;gpu->table=std::move(table);gpu->assets=task.assets;
                auto& binding=gpu->binding;binding.key=task.source->key();binding.pbr=batch.pbr_slot_count!=0;binding.double_sided=batch.double_sided;
                for(auto& handle:binding.geometry)handle=batch.vertices->nativeHandle();
                const bool normal=!binding.pbr && batch.materialSlots()[0].normal_texture;
                const bool specular=!binding.pbr && batch.materialSlots()[0].specular_texture;
                binding.offsets={batch.position_offset,batch.normal_offset,batch.texcoord_offset,batch.color_offset,
                    binding.pbr||normal?batch.tangent_offset:batch.normal_offset,
                    normal?batch.texcoord1_offset:batch.texcoord_offset,specular?batch.texcoord2_offset:batch.texcoord_offset,batch.weight_offset};
                const auto& source=*batch.native_material_source;
                const std::array<std::shared_ptr<const ALMetalBuffer>,3> numeric{source.materialLease(),source.pbrLease(false),source.pbrLease(true)};
                for(unsigned at=0;at<3;++at)binding.numeric[at]=numeric[at]?numeric[at]->nativeHandle():nullptr;
                binding.indices=batch.indices->nativeHandle();binding.index_offset=batch.index_offset;binding.index_count=batch.index_count;binding.index_type=batch.index_type;
                binding.policy=buffer->value.get();binding.bones=buffer->value.get();binding.bone_offset=96;binding.bone_count=bones.size();binding.qualified=task.source->boundsQualified();
                // Capture each graph generation while the exact immutable
                // native tuple is pinned; later binds never query live wrappers.
                for(const auto* item:{batch.vertices,batch.indices,source.materialLease().get(),
                    source.pbrLease(false).get(),source.pbrLease(true).get(),buffer->value.get()})if(item)
                {gpu->plan_complete&=bool(item->resourceView());appendPlanView(gpu->plan_views,item->resourceView());}
                const auto graph_bytes=gpu->plan_views.capacity()*sizeof(RP::View)+
                    gpu->assets.capacity()*sizeof(ALMetalWorldSourceNativeFootprint)+sizeof(DeclarationMetadata)+2*sizeof(void*);
                const auto live_metadata=state.context->budget->bytes.load(std::memory_order_relaxed);
                if(live_metadata>state.context->budget->limit||graph_bytes>state.context->budget->limit-live_metadata)return false;
                auto graph_metadata=std::make_shared<DeclarationMetadata>();graph_metadata->budget=state.context->budget;
                graph_metadata->bytes=graph_bytes;state.context->budget->bytes.fetch_add(graph_bytes,std::memory_order_relaxed);
                gpu->graph_metadata=std::move(graph_metadata);
                task.gpu=std::shared_ptr<const ALMetalWorldAnimatedGpuSource>(new ALMetalWorldAnimatedGpuSource(std::move(gpu)));
                // All allocating nodes are staged before the cache entry or
                // asset reference ledger changes. Failed optional publication
                // cannot leave uncharged native roots in an idle entry.
                decltype(state.animated) staged;
                staged.emplace(task.source.get(),Impl::AnimatedEntry{task.source,task.native,task.gpu});
                std::vector<void*> inserted_assets;inserted_assets.reserve(task.gpu->assets().size());
                try
                {
                    for(const auto& asset:task.gpu->assets())
                    {
                        auto [it,inserted]=state.cohort_assets.try_emplace(asset.allocation);
                        if(inserted)inserted_assets.push_back(asset.allocation);
                    }
                    auto node=staged.extract(staged.begin());
                    if(!state.animated.insert(std::move(node)).inserted)
                    {for(auto native:inserted_assets)state.cohort_assets.erase(native);return false;}
                }
                catch(...){for(auto native:inserted_assets)state.cohort_assets.erase(native);throw;}
                for(const auto& asset:task.gpu->assets())
                {
                    auto& accounting=state.cohort_assets.find(asset.allocation)->second;
                    if(!accounting.references){accounting.bytes=asset.bytes;state.cohort_asset_bytes+=asset.bytes;}
                    ++accounting.references;
                }
                ++state.statistics.animated_source_publications;state.statistics.encoded_row_bytes+=bytes;
            }
        }
        else ++state.statistics.animated_source_hits;
        output.push_back({task.gpu,std::move(task.pose),std::move(task.native),task.view});
    }
    return true;
}
catch(const std::bad_alloc&){output.clear();if(error)error->clear();return false;}
catch(...){output.clear();if(error)error->clear();return false;}

void ALMetalWorldSourcePages::invalidate(std::uint64_t identity,std::uint64_t minimum_revision)noexcept
{
    ++mImpl->epoch;
    for(auto it=mImpl->animated.begin();it!=mImpl->animated.end();)
        if(it->second.gpu->binding().key.identity==identity){auto old=it++;mImpl->eraseAnimated(old);}else ++it;
    const auto found=mImpl->identities.find(identity);if(found==mImpl->identities.end())return;
    for(auto slot:found->second)
    {
        auto& row=mImpl->rows[slot];row.valid=false;mImpl->changed(slot);
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();row.table.reset();
        const auto next=minimum_revision?minimum_revision:(row.revision==std::numeric_limits<std::uint64_t>::max()?row.revision:row.revision+1);
        row.minimum_revision=std::max(row.minimum_revision,next);
    }
    mImpl->pruneCohorts();
}
void ALMetalWorldSourcePages::withdraw(std::uint64_t identity)noexcept
{
    ++mImpl->epoch;
    for(auto it=mImpl->animated.begin();it!=mImpl->animated.end();)
        if(it->second.gpu->binding().key.identity==identity){auto old=it++;mImpl->eraseAnimated(old);}else ++it;
    const auto found=mImpl->identities.find(identity);if(found==mImpl->identities.end())return;
    for(auto slot:found->second)
    {
        auto& row=mImpl->rows[slot];row.valid=false;mImpl->changed(slot);
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();row.table.reset();
    }
    mImpl->pruneCohorts();
}
void ALMetalWorldSourcePages::clear()noexcept
{
    ++mImpl->epoch;
    mImpl->animated.clear();
    mImpl->cohorts.clear();mImpl->cohort_rows=mImpl->cohort_metadata=mImpl->cohort_references=0;mImpl->cohort_assets.clear();mImpl->cohort_asset_bytes=0;
    mImpl->lookup.clear();mImpl->identities.clear();mImpl->free.clear();
    for(std::size_t slot=mImpl->rows.size();slot>0;--slot)
    {auto& row=mImpl->rows[slot-1];row.valid=false;row.allocated=false;row.revision=0;row.minimum_revision=0;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();row.table.reset();
        if(row.generation!=std::numeric_limits<std::uint32_t>::max())mImpl->free.push_back(static_cast<std::uint32_t>(slot-1));}
}
void ALMetalWorldSourcePages::trimUnusedCohorts()noexcept{mImpl->pruneCohorts();mImpl->pruneAnimated();}
ALMetalWorldSourcePageStats ALMetalWorldSourcePages::stats()const noexcept
{
    auto result=mImpl->statistics;result.rows=mImpl->lookup.size();result.binds=mImpl->context->binds.load(std::memory_order_relaxed);
    result.cohort_declaration_calls=mImpl->context->cohort_declaration_calls.load(std::memory_order_relaxed);
    result.cohort_declared_resources=mImpl->context->cohort_declared_resources.load(std::memory_order_relaxed);
    result.device_proof_entries=mImpl->device_proofs.size();
    result.live_direct_slots=mImpl->context->direct_slots->count.load(std::memory_order_relaxed);
    result.live_direct_slot_bytes=mImpl->context->direct_slots->bytes.load(std::memory_order_relaxed);
    result.cached_cohort_native_asset_bytes=mImpl->cohort_asset_bytes;
    result.cached_cohort_native_asset_limit=mImpl->limits.maximum_cached_cohort_native_asset_bytes;
    result.cached_compositions=mImpl->cohorts.size();result.cached_cohorts=mImpl->cohort_references;result.cached_cohort_rows=mImpl->cohort_rows;result.cached_cohort_metadata_bytes=mImpl->cohort_metadata;
    result.live_metadata_bytes=mImpl->context->budget->bytes.load(std::memory_order_relaxed)+mImpl->context->resources->stats().live_buffer_bytes;
    return result;
}
void ALMetalWorldSourcePages::failNextPublicationForTesting()noexcept{mImpl->force_failure=true;}
void ALMetalWorldSourcePages::failNextCohortCacheInsertionForTesting()noexcept{mImpl->force_cache_failure=true;}
