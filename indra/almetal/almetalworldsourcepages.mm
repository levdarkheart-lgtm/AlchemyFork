/**
 * @file almetalworldsourcepages.mm
 * @brief Reflection-encoded source pages with explicit actual-view readers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldsourcepages.h"
#include "almetalworldprepare.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <list>
#include <new>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace
{
void fail(std::string* error,const char* message){if(error)*error=message;}
struct Budget {std::atomic<std::size_t> bytes{0};std::size_t limit=0;};
struct Buffer
{
    std::shared_ptr<const ALMetalBuffer> value;
    std::shared_ptr<Budget> budget;
    ~Buffer(){if(value)budget->bytes.fetch_sub(value->length(),std::memory_order_relaxed);}
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
struct Row
{
    Key key;
    std::uint64_t revision=0,minimum_revision=0,last_use=0;
    std::uint32_t generation=0;
    bool allocated=false,valid=false;
    Signature signature;
    std::list<std::uint32_t>::iterator lru;
    std::weak_ptr<const ALMetalWorldMaterialSource> material;
    std::weak_ptr<const ALMetalWorldResourceSource> resources;
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
    std::atomic<std::uint64_t> binds{0};
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
    result->budget=context->budget;
    context->budget->bytes.fetch_add(length,std::memory_order_relaxed);
    return result;
}
bool finite(const float* values,std::size_t count)
{for(std::size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
}

struct ALMetalWorldSourceFrameView::Impl
{
    std::shared_ptr<Context> context;
    std::shared_ptr<BankReader> bank;
    std::vector<std::shared_ptr<PageReader>> pages;
    std::shared_ptr<Buffer> regions,globals,mapping;
    std::vector<std::uint32_t> rows,generations;
    std::vector<std::shared_ptr<const void>> native_owners;
    std::vector<std::shared_ptr<const ALMetalBuffer>> buffers;
    std::vector<std::shared_ptr<const ALMetalTexture>> textures;
    std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> tables;
    std::vector<void*> geometry;
    __weak id<MTLRenderCommandEncoder> declared_encoder=nil;
};
ALMetalWorldSourceFrameView::ALMetalWorldSourceFrameView(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceFrameView::~ALMetalWorldSourceFrameView()=default;
std::size_t ALMetalWorldSourceFrameView::size()const noexcept{return mImpl->rows.size();}
std::uint32_t ALMetalWorldSourceFrameView::rowId(std::size_t index)const noexcept
{return index<mImpl->rows.size()?mImpl->rows[index]:std::numeric_limits<std::uint32_t>::max();}
std::uint32_t ALMetalWorldSourceFrameView::allocationGeneration(std::size_t index)const noexcept
{return index<mImpl->generations.size()?mImpl->generations[index]:0;}
bool ALMetalWorldSourceFrameView::bind(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,std::string* error)const
try
{
    auto& view=*mImpl;
    if(command.isCommitted()||command.nativeDevice()!=(__bridge void*)view.context->native ||
       encoder.nativeCommandBufferForIndirect()!=command.nativeHandle())
    {fail(error,"Native source view requires its exact uncommitted device command");return false;}
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForResources();
    if(!native){fail(error,"Native source view requires a live render encoder");return false;}
    const auto declare=[&](const ALMetalBuffer& buffer,MTLRenderStages stages)
    {[native useResource:(__bridge id<MTLBuffer>)buffer.nativeHandle() usage:MTLResourceUsageRead stages:stages];};
    if(view.declared_encoder!=native)
    {
        if(!command.retainUntilCompletion(shared_from_this(),error))return false;
        declare(*view.bank->bank->buffer->value,MTLRenderStageVertex|MTLRenderStageFragment);
        for(const auto& page:view.pages)declare(*page->page->buffer->value,MTLRenderStageVertex|MTLRenderStageFragment);
        for(const auto& buffer:view.buffers)declare(*buffer,MTLRenderStageVertex|MTLRenderStageFragment);
        for(auto handle:view.geometry)
            [native useResource:(__bridge id<MTLBuffer>)handle usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        for(const auto& table:view.tables)
            if(!view.context->resources->declareResources(encoder,command,table,error))return false;
        view.declared_encoder=native;
    }
    const bool bound=encoder.setVertexBuffer(0,*view.bank->bank->buffer->value,0,error) &&
        encoder.setFragmentBuffer(0,*view.bank->bank->buffer->value,0,error) &&
        encoder.setVertexBuffer(1,*view.regions->value,0,error) &&
        encoder.setVertexBuffer(2,*view.globals->value,0,error) &&
        encoder.setFragmentBuffer(2,*view.globals->value,0,error) &&
        encoder.setVertexBuffer(3,*view.mapping->value,0,error);
    if(bound)view.context->binds.fetch_add(1,std::memory_order_relaxed);
    return bound;
}
catch(const std::bad_alloc&){if(error)error->clear();return false;}

struct ALMetalWorldSourcePages::Impl
{
    std::shared_ptr<Context> context;
    ALMetalWorldSourcePageLimits limits;
    std::vector<Row> rows;
    std::vector<std::uint32_t> free;
    std::unordered_map<Key,std::uint32_t,KeyHash> lookup;
    std::unordered_map<std::uint64_t,std::vector<std::uint32_t>> identities;
    std::list<std::uint32_t> lru;
    std::array<std::shared_ptr<Page>,alMetalWorldSourceMaximumRows/alMetalWorldSourceRowsPerPage> pages;
    std::shared_ptr<Bank> bank;
    ALMetalWorldSourcePageStats statistics;
    std::uint64_t serial=0;
    bool supported=false,force_failure=false;

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
    impl->limits=limits;context.device=&device;context.native=(__bridge id<MTLDevice>)device.nativeHandle();
    context.library=(__bridge id<MTLLibrary>)library.nativeHandle();context.anisotropy=anisotropy;
    if(context.library.device!=context.native || context.native.argumentBuffersSupport!=MTLArgumentBuffersTier2)
    {fail(error,"Native source rows require an exact Tier2 shader context");return {};}
    auto function=[context.library newFunctionWithName:@"almetal_world_material_source_rows_vertex"];
    context.bank_encoder=[function newArgumentEncoderWithBufferIndex:0];
    context.row_encoder=[context.bank_encoder newArgumentEncoderForBufferAtIndex:0];
    const auto length=context.row_encoder.encodedLength,alignment=context.row_encoder.alignment;
    context.bank_length=context.bank_encoder.encodedLength;
    if(!length||!alignment||length>std::numeric_limits<std::size_t>::max()-(alignment-1)||!context.bank_length)
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
try
{
    auto& state=*mImpl;
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldSourceFrameView>
    {++state.statistics.failures;fail(error,message);return {};};
    if(!state.supported||requests.empty()||regions.empty()||!finite(globals.mirror_clip_plane,4)||
       globals.flags[0]>1||globals.flags[1]>1||globals.flags[2]>1||globals.flags[3])return reject("Native source view shape is invalid");
    for(const auto& region:regions)
        if(!finite(region.modelview_projection,16)||!finite(region.modelview,16)||region.modelview[15]!=1 ||
           !finite(&region.normal_matrix[0][0],12))return reject("Native source regional matrices are invalid");
    if(state.force_failure){state.force_failure=false;return reject("Forced native source publication failure");}
    auto view=std::make_unique<ALMetalWorldSourceFrameView::Impl>();view->context=state.context;
    view->rows.reserve(requests.size());view->generations.reserve(requests.size());
    std::vector<std::uint32_t> mapping(state.limits.maximum_rows,std::numeric_limits<std::uint32_t>::max());
    std::array<bool,alMetalWorldSourceMaximumRows/alMetalWorldSourceRowsPerPage> used_pages{};
    std::set<std::shared_ptr<const void>,std::owner_less<std::shared_ptr<const void>>> owners;
    std::unordered_set<const ALMetalBuffer*> buffers;
    std::unordered_set<const ALMetalTexture*> textures;
    std::unordered_set<const ALMetalWorldResourceTable*> tables;
    std::unordered_set<void*> geometry;
    if(++state.serial==0)++state.serial;
    std::uint32_t highest_slot=0;
    for(const auto& request:requests)
    {
        ++state.statistics.requests;
        if(!request.key.identity||!request.key.revision||!request.source||!request.native.owner || request.region>=regions.size())
            return reject("Native source request is incomplete");
        const auto& source=*request.source;
        const bool pbr=source.pbr_slot_count!=0,material=source.material_slot_count!=0;
        if(pbr==material || source.normal_texture || source.skin_palette || source.skin_joint_count||source.weight_offset ||
           source.avatar_rigid||source.avatar_skinning||source.avatar_cloth||source.terrain||source.impostor ||
           !source.native_material_source||!source.native_material_source->matches(source)||
           !source.native_resource_source||!source.native_resource_source->matches(source) ||
           (request.native.vertices && request.native.vertices.get()!=source.vertices)||
           (request.native.indices && request.native.indices.get()!=source.indices))
            return reject("Native source request lacks an exact static material generation");
        ALMetalWorldViewMatrices matrices;
        std::memcpy(matrices.modelview_projection,regions[request.region].modelview_projection,64);
        std::memcpy(matrices.modelview,regions[request.region].modelview,64);
        std::memcpy(matrices.normal_matrix,regions[request.region].normal_matrix,48);
        for(unsigned axis=0;axis<3;++axis)matrices.rigged_normal_matrix[axis][axis]=1;
        std::memcpy(matrices.texture_matrix,source.texture_matrix,64);
        ALMetalWorldGBufferScene validation;validation.normal_hdr=globals.flags[2];validation.width=validation.height=1;
        if(!alMetalValidateWorldBatch(validation,ALMetalWorldDraw(source,&matrices),error))
        {++state.statistics.failures;return {};}
        auto table=state.context->resources->prepare(source.native_resource_source,*state.context->sampler,error);
        if(!table){++state.statistics.failures;return {};}
        std::array<std::shared_ptr<const ALMetalBuffer>,3> numeric{
            source.native_material_source->materialLease(),source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)};
        auto arguments=table->argumentLease();
        Signature signature;
        signature.offsets={source.position_offset,source.normal_offset,source.texcoord_offset,source.color_offset,
            pbr||static_cast<const ALMetalWorldSimpleBatch&>(source).materialSlots()[0].normal_texture?source.tangent_offset:source.normal_offset,
            material&&static_cast<const ALMetalWorldSimpleBatch&>(source).materialSlots()[0].normal_texture?source.texcoord1_offset:source.texcoord_offset,
            material&&static_cast<const ALMetalWorldSimpleBatch&>(source).materialSlots()[0].specular_texture?source.texcoord2_offset:source.texcoord_offset};
        for(unsigned binding=0;binding<7;++binding)signature.handles[binding]=source.vertices->nativeHandle();
        for(unsigned binding=0;binding<3;++binding)signature.handles[7+binding]=numeric[binding]?numeric[binding]->nativeHandle():nullptr;
        signature.handles[pbr?11:10]=arguments->nativeHandle();
        for(unsigned value=0;value<16;++value)signature.values[value]=std::bit_cast<std::uint32_t>(source.texture_matrix[value]);
        signature.values[16]=std::bit_cast<std::uint32_t>(source.alpha_cutoff);
        signature.values[17]=std::bit_cast<std::uint32_t>(source.use_vertex_color?1.f:0.f);
        signature.values[18]=std::bit_cast<std::uint32_t>(source.alpha_cutoff_bias?1.f:0.f);
        signature.values[19]=std::bit_cast<std::uint32_t>(source.alpha_mask?1.f:0.f);signature.values[20]=source.texture_count;
        const Key key{request.key.identity,request.key.flavor};
        auto at=state.lookup.find(key);std::uint32_t slot;
        if(at==state.lookup.end())
        {
            if(state.free.empty())
            {
                // Only current-view selected slots are protected here. Old
                // queued/GPU views keep immutable pages/banks and force COW;
                // they must not permanently reserve the online source IDs.
                std::uint32_t victim=std::numeric_limits<std::uint32_t>::max();
                for(std::size_t checked=0;checked<state.rows.size();++checked)
                {
                    const auto candidate=state.lru.front();
                    // Every selected row is already spliced to the tail, so
                    // encountering one proves all remaining rows are selected
                    // or have exhausted their allocation-generation space.
                    if(mapping[candidate]!=std::numeric_limits<std::uint32_t>::max())break;
                    if(state.rows[candidate].generation!=std::numeric_limits<std::uint32_t>::max())
                    {victim=candidate;break;}
                    state.lru.splice(state.lru.end(),state.lru,state.rows[candidate].lru);
                }
                if(victim==std::numeric_limits<std::uint32_t>::max())return reject("Native source row capacity is exhausted");
                auto& old=state.rows[victim];state.lookup.erase(old.key);
                const auto old_identity=state.identities.find(old.key.identity);
                if(old_identity!=state.identities.end())
                {
                    std::erase(old_identity->second,victim);
                    if(old_identity->second.empty())state.identities.erase(old_identity);
                }
                old.valid=false;old.allocated=false;old.revision=old.minimum_revision=0;
                if(auto source=old.resources.lock())state.context->resources->releaseSourceCache(source);
                old.material.reset();old.resources.reset();state.free.push_back(victim);
            }
            slot=state.free.back();
            auto& identity=state.identities[key.identity];identity.push_back(slot);
            try{state.lookup.emplace(key,slot);}catch(...){identity.pop_back();throw;}
            state.free.pop_back();auto& row=state.rows[slot];row.key=key;row.allocated=true;row.valid=false;
            ++row.generation;
        }
        else slot=at->second;
        auto& row=state.rows[slot];
        if(request.key.revision<row.revision||request.key.revision<row.minimum_revision)return reject("Native source row publication is stale");
        if(mapping[slot]!=std::numeric_limits<std::uint32_t>::max()&&mapping[slot]!=request.region)
            return reject("Native source row has conflicting view-region selection");
        if(mapping[slot]!=std::numeric_limits<std::uint32_t>::max()&&
            (row.revision!=request.key.revision||row.signature!=signature))
            return reject("Native source row has conflicting immutable inputs");
        mapping[slot]=request.region;
        const bool hit=row.valid&&row.revision==request.key.revision&&row.signature==signature &&
            !row.material.expired()&&!row.resources.expired();
        if(!hit)
        {
            // No actual reader may see these bytes change. Idle bank/cache
            // shared_ptrs do not count; only FrameView reader tokens do.
            row.valid=false;
            if(!state.preparePage(slot/alMetalWorldSourceRowsPerPage,error)){++state.statistics.failures;return {};}
            auto& page=state.pages[slot/alMetalWorldSourceRowsPerPage];
            [state.context->row_encoder setArgumentBuffer:(__bridge id<MTLBuffer>)page->buffer->value->nativeHandle()
                offset:(slot%alMetalWorldSourceRowsPerPage)*state.context->stride];
            for(unsigned binding=0;binding<7;++binding)
                [state.context->row_encoder setBuffer:(__bridge id<MTLBuffer>)signature.handles[binding] offset:signature.offsets[binding] atIndex:binding];
            for(unsigned binding=7;binding<12;++binding)
                [state.context->row_encoder setBuffer:(__bridge id<MTLBuffer>)signature.handles[binding] offset:0 atIndex:binding];
            std::memcpy([state.context->row_encoder constantDataAtIndex:12],source.texture_matrix,64);
            std::memcpy([state.context->row_encoder constantDataAtIndex:13],signature.values.data()+16,16);
            std::memcpy([state.context->row_encoder constantDataAtIndex:14],signature.values.data()+20,16);
            [state.context->row_encoder setArgumentBuffer:nil offset:0];
            row.signature=signature;row.revision=request.key.revision;
            row.material=source.native_material_source;row.resources=source.native_resource_source;row.valid=true;
            ++state.statistics.row_publications;state.statistics.encoded_row_bytes+=state.context->row_encoder.encodedLength;
        }
        else ++state.statistics.hits;
        row.last_use=state.serial;highest_slot=std::max(highest_slot,slot);
        state.lru.splice(state.lru.end(),state.lru,row.lru);
        view->rows.push_back(slot);view->generations.push_back(row.generation);used_pages[slot/alMetalWorldSourceRowsPerPage]=true;
        if(owners.insert(request.native.owner).second)view->native_owners.push_back(request.native.owner);
        for(auto handle:{source.vertices->nativeHandle(),source.indices->nativeHandle()})
            if(geometry.insert(handle).second)view->geometry.push_back(handle);
        for(const auto& buffer:{request.native.vertices,request.native.indices})
            if(buffer&&buffers.insert(buffer.get()).second)view->buffers.push_back(buffer);
        for(const auto& buffer:numeric)
            if(buffer&&buffers.insert(buffer.get()).second)view->buffers.push_back(buffer);
        for(const auto& texture:source.native_resource_source->generationLeases())
            if(texture&&textures.insert(texture.get()).second)view->textures.push_back(texture);
        if(tables.insert(table.get()).second)view->tables.push_back(table);
    }
    mapping.resize(std::size_t(highest_slot)+1);
    if(regions.size()>std::numeric_limits<std::size_t>::max()/sizeof(ALMetalWorldSourceRegion))return reject("Native source regions overflow");
    view->regions=makeBuffer(state.context,regions.size_bytes(),"Alchemy source regional view matrices",error);
    view->globals=makeBuffer(state.context,sizeof(globals),"Alchemy source view globals",error);
    view->mapping=makeBuffer(state.context,mapping.size()*sizeof(std::uint32_t),"Alchemy source to view region map",error);
    if(!view->regions||!view->globals||!view->mapping){++state.statistics.failures;return {};}
    std::memcpy(view->regions->value->mappedBytes(),regions.data(),regions.size_bytes());
    std::memcpy(view->globals->value->mappedBytes(),&globals,sizeof(globals));
    std::memcpy(view->mapping->value->mappedBytes(),mapping.data(),mapping.size()*sizeof(std::uint32_t));
    for(std::size_t page=0;page<used_pages.size();++page)if(used_pages[page])
    {
        auto reader=state.pages[page]->reader.lock();
        if(!reader){reader=std::make_shared<PageReader>();reader->page=state.pages[page];state.pages[page]->reader=reader;}
        view->pages.push_back(std::move(reader));
    }
    view->bank=state.bank->reader.lock();
    if(!view->bank){view->bank=std::make_shared<BankReader>();view->bank->bank=state.bank;state.bank->reader=view->bank;}
    state.statistics.view_bytes+=regions.size_bytes()+sizeof(globals)+mapping.size()*sizeof(std::uint32_t);
    return std::shared_ptr<const ALMetalWorldSourceFrameView>(new ALMetalWorldSourceFrameView(std::move(view)));
}
catch(const std::bad_alloc&){++mImpl->statistics.failures;if(error)error->clear();return {};}
void ALMetalWorldSourcePages::invalidate(std::uint64_t identity,std::uint64_t minimum_revision)noexcept
{
    const auto found=mImpl->identities.find(identity);if(found==mImpl->identities.end())return;
    for(auto slot:found->second)
    {
        auto& row=mImpl->rows[slot];row.valid=false;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();
        const auto next=minimum_revision?minimum_revision:(row.revision==std::numeric_limits<std::uint64_t>::max()?row.revision:row.revision+1);
        row.minimum_revision=std::max(row.minimum_revision,next);
    }
}
void ALMetalWorldSourcePages::withdraw(std::uint64_t identity)noexcept
{
    const auto found=mImpl->identities.find(identity);if(found==mImpl->identities.end())return;
    for(auto slot:found->second)
    {
        auto& row=mImpl->rows[slot];row.valid=false;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();
    }
}
void ALMetalWorldSourcePages::clear()noexcept
{
    mImpl->lookup.clear();mImpl->identities.clear();mImpl->free.clear();
    for(std::size_t slot=mImpl->rows.size();slot>0;--slot)
    {auto& row=mImpl->rows[slot-1];row.valid=false;row.allocated=false;row.revision=0;row.minimum_revision=0;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();
        if(row.generation!=std::numeric_limits<std::uint32_t>::max())mImpl->free.push_back(static_cast<std::uint32_t>(slot-1));}
}
ALMetalWorldSourcePageStats ALMetalWorldSourcePages::stats()const noexcept
{
    auto result=mImpl->statistics;result.rows=mImpl->lookup.size();result.binds=mImpl->context->binds.load(std::memory_order_relaxed);
    result.live_metadata_bytes=mImpl->context->budget->bytes.load(std::memory_order_relaxed)+mImpl->context->resources->stats().live_buffer_bytes;
    return result;
}
void ALMetalWorldSourcePages::failNextPublicationForTesting()noexcept{mImpl->force_failure=true;}
