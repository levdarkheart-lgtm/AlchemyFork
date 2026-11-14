/**
 * @file almetalworldsourcepages.mm
 * @brief Reflection-encoded source pages with explicit actual-view readers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldsourcepages.h"
#include "almetalworldprepare.h"
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
                  const ALMetalWorldSourceGlobals& globals,SourceInput& input,std::string* error)
{
    if(!request.source||!request.native.owner)return false;
    const auto& source=*request.source;
    const bool pbr=source.pbr_slot_count!=0,material=source.material_slot_count!=0;
    if(pbr==material||source.normal_texture||source.skin_palette||source.skin_joint_count||source.weight_offset||
       source.avatar_rigid||source.avatar_skinning||source.avatar_cloth||source.terrain||source.impostor||
       !source.native_material_source||!source.native_material_source->matches(source)||
       !source.native_resource_source||!source.native_resource_source->matches(source)||
       (request.native.vertices&&request.native.vertices.get()!=source.vertices)||
       (request.native.indices&&request.native.indices.get()!=source.indices))
    {fail(error,"Native source request lacks an exact static material generation");return false;}
    ALMetalWorldViewMatrices matrices;
    std::memcpy(matrices.modelview_projection,region.modelview_projection,64);
    std::memcpy(matrices.modelview,region.modelview,64);std::memcpy(matrices.normal_matrix,region.normal_matrix,48);
    for(unsigned axis=0;axis<3;++axis)matrices.rigged_normal_matrix[axis][axis]=1;
    std::memcpy(matrices.texture_matrix,source.texture_matrix,64);
    ALMetalWorldGBufferScene validation;validation.normal_hdr=globals.flags[2];validation.width=validation.height=1;
    if(!alMetalValidateWorldBatch(validation,ALMetalWorldDraw(source,&matrices),error))return false;
    input.material=source.native_material_source;input.resources=source.native_resource_source;input.pbr=pbr;
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
    for(auto bytes:{encoded_row_bytes,encoded_table_bytes,encoded_bank_range_bytes,copied_page_bytes,copied_bank_bytes,zeroed_page_bytes,zeroed_bank_bytes})
    {if(bytes>std::numeric_limits<std::uint64_t>::max()-total)return std::numeric_limits<std::uint64_t>::max();total+=bytes;}
    return total;
}

struct ALMetalWorldSourceFrameView::Impl
{
    struct CommandSource
    {
        ALMetalWorldSourceKey key;
        const ALMetalBuffer* indices=nullptr;
        void* native_indices=nullptr;
    };
    std::shared_ptr<Context> context;
    std::shared_ptr<BankReader> bank;
    std::vector<std::shared_ptr<PageReader>> pages;
    std::shared_ptr<Buffer> regions,globals,mapping;
    std::vector<std::uint32_t> rows,generations;
    std::vector<CommandSource> commands;
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
bool ALMetalWorldSourceFrameView::matchesSourceCommand(std::size_t index,const ALMetalWorldSourceKey& key,
                                                     const ALMetalBuffer& indices)const noexcept
{
    if(index>=mImpl->commands.size())return false;
    const auto& source=mImpl->commands[index];
    return source.key.identity==key.identity&&source.key.flavor==key.flavor&&source.key.revision==key.revision&&
        source.indices==&indices&&source.native_indices==indices.nativeHandle();
}
std::weak_ptr<const ALMetalWorldResourceTable> ALMetalWorldSourceFrameView::tableGenerationForTesting(std::size_t index)const noexcept
{return index<mImpl->tables.size()?std::weak_ptr<const ALMetalWorldResourceTable>(mImpl->tables[index]):std::weak_ptr<const ALMetalWorldResourceTable>{};}
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
        if(!view.geometry.empty())
        {
            const auto* geometry=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(view.geometry.data());
            [native useResources:geometry count:view.geometry.size() usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        }
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
    ALMetalWorldSourcePageStats statistics;
    std::uint64_t serial=0;
    bool supported=false,force_failure=false;
    std::shared_ptr<ALMetalWorldSourcePages::PublicationAdmission> admission;
    std::uint64_t epoch=0;

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
    impl->limits=limits;context.device=&device;context.native=(__bridge id<MTLDevice>)device.nativeHandle();
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
void ALMetalWorldSourcePages::setPublicationAdmission(PublicationAdmission admission)
{
    auto holder=admission?std::make_shared<PublicationAdmission>(std::move(admission)):nullptr;
    mImpl->admission=std::move(holder);++mImpl->epoch;
}
std::shared_ptr<const ALMetalWorldSourceFrameView> ALMetalWorldSourcePages::freezeInternal(
    ALMetalCommandBuffer* command,std::span<const ALMetalWorldSourceRequest> requests,
    std::span<const ALMetalWorldSourceRegion> regions,const ALMetalWorldSourceGlobals& globals,std::string* error)
try
{
    auto& state=*mImpl;
    const auto reject=[&](const char* message)->std::shared_ptr<const ALMetalWorldSourceFrameView>
    {++state.statistics.failures;fail(error,message);return {};};
    const auto commandValid=[&]{return !command||(!command->isCommitted()&&command->nativeDevice()==(__bridge void*)state.context->native);};
    if(!state.supported||requests.empty()||requests.size()>alMetalWorldSourceMaximumRows||regions.empty()||
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
    rows.reserve(requests.size());request_rows.reserve(requests.size());selected.reserve(requests.size());
    const auto epoch=state.epoch;
    std::uint32_t highest_slot=0;
    std::unordered_set<void*> proven_buffers;
    proven_buffers.reserve(std::min(requests.size(),state.limits.maximum_rows)*5);
    // Read-only plan: no row/LRU/cache mutation, GPU allocation or mapped write.
    for(const auto& request:requests)
    {
        ++state.statistics.requests;
        if(!request.key.identity||!request.key.revision||request.region>=regions.size())return reject("Native source request is incomplete");
        SourceInput input;if(!captureInput(request,regions[request.region],globals,input,error)){++state.statistics.failures;return {};}
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
            !current.material.expired()&&!current.resources.expired()&&same_table;
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
    const auto view_bytes=regions.size_bytes()+sizeof(globals)+mapping.size()*sizeof(std::uint32_t);
    const auto page_allocation=work.allocation_bytes-work.encoded_table_bytes;
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
            if(!captureInput(current,regions[current.region],globals,now,error)||!sameInput(now,row.input))
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
            row.valid=false;
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
    view->regions=makeBuffer(state.context,regions.size_bytes(),"Alchemy source regional view matrices",error);
    view->globals=makeBuffer(state.context,sizeof(globals),"Alchemy source view globals",error);
    view->mapping=makeBuffer(state.context,mapping.size()*sizeof(std::uint32_t),"Alchemy source to view region map",error);
    if(!view->regions||!view->globals||!view->mapping){++state.statistics.failures;return {};}
    std::memcpy(view->regions->value->mappedBytes(),regions.data(),regions.size_bytes());
    std::memcpy(view->globals->value->mappedBytes(),&globals,sizeof(globals));
    std::memcpy(view->mapping->value->mappedBytes(),mapping.data(),mapping.size()*sizeof(std::uint32_t));
    std::set<std::shared_ptr<const void>,std::owner_less<std::shared_ptr<const void>>> owners;
    std::unordered_set<const ALMetalBuffer*> buffers;std::unordered_set<const ALMetalTexture*> textures;std::unordered_set<void*> geometry;
    view->rows.reserve(requests.size());view->generations.reserve(requests.size());view->commands.reserve(requests.size());
    for(std::size_t index=0;index<requests.size();++index)
    {
        const auto& planned=rows[request_rows[index]];const auto& request=planned.request;
        view->rows.push_back(planned.slot);view->generations.push_back(planned.generation);
        view->commands.push_back({request.key,planned.input.indices,planned.input.indices->nativeHandle()});
        if(owners.insert(request.native.owner).second)view->native_owners.push_back(request.native.owner);
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
    state.statistics.view_bytes+=view_bytes;
    return std::shared_ptr<const ALMetalWorldSourceFrameView>(new ALMetalWorldSourceFrameView(std::move(view)));
}
catch(const std::bad_alloc&){++mImpl->statistics.failures;if(error)error->clear();return {};}
void ALMetalWorldSourcePages::invalidate(std::uint64_t identity,std::uint64_t minimum_revision)noexcept
{
    ++mImpl->epoch;
    const auto found=mImpl->identities.find(identity);if(found==mImpl->identities.end())return;
    for(auto slot:found->second)
    {
        auto& row=mImpl->rows[slot];row.valid=false;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();row.table.reset();
        const auto next=minimum_revision?minimum_revision:(row.revision==std::numeric_limits<std::uint64_t>::max()?row.revision:row.revision+1);
        row.minimum_revision=std::max(row.minimum_revision,next);
    }
}
void ALMetalWorldSourcePages::withdraw(std::uint64_t identity)noexcept
{
    ++mImpl->epoch;
    const auto found=mImpl->identities.find(identity);if(found==mImpl->identities.end())return;
    for(auto slot:found->second)
    {
        auto& row=mImpl->rows[slot];row.valid=false;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();row.table.reset();
    }
}
void ALMetalWorldSourcePages::clear()noexcept
{
    ++mImpl->epoch;
    mImpl->lookup.clear();mImpl->identities.clear();mImpl->free.clear();
    for(std::size_t slot=mImpl->rows.size();slot>0;--slot)
    {auto& row=mImpl->rows[slot-1];row.valid=false;row.allocated=false;row.revision=0;row.minimum_revision=0;
        if(auto source=row.resources.lock())mImpl->context->resources->releaseSourceCache(source);
        row.material.reset();row.resources.reset();row.table.reset();
        if(row.generation!=std::numeric_limits<std::uint32_t>::max())mImpl->free.push_back(static_cast<std::uint32_t>(slot-1));}
}
ALMetalWorldSourcePageStats ALMetalWorldSourcePages::stats()const noexcept
{
    auto result=mImpl->statistics;result.rows=mImpl->lookup.size();result.binds=mImpl->context->binds.load(std::memory_order_relaxed);
    result.device_proof_entries=mImpl->device_proofs.size();
    result.live_metadata_bytes=mImpl->context->budget->bytes.load(std::memory_order_relaxed)+mImpl->context->resources->stats().live_buffer_bytes;
    return result;
}
void ALMetalWorldSourcePages::failNextPublicationForTesting()noexcept{mImpl->force_failure=true;}
