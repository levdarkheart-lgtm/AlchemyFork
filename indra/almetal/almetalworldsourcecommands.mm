/**
 * @file almetalworldsourcecommands.mm
 * @brief Persistent indexed ICBs with bounded native-only ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldsourcecommands.h"
#include <Metal/Metal.h>
#include <algorithm>
#include <atomic>
#include <list>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

const char* alMetalWorldSourceCommandFailureName(ALMetalWorldSourceCommandFailure value) noexcept
{
    static constexpr const char* names[]{"none","unsupported","validation","capacity","admission",
        "native_allocation","host_allocation","retention","execution","pipeline","short_run","source_setup"};
    const auto index=static_cast<std::size_t>(value);
    return index<std::size(names)?names[index]:"invalid";
}
namespace
{
struct Accounting
{
    std::atomic<std::uint64_t> icb_bytes{0},index_bytes{0},binding_bytes{0};
    std::atomic<std::size_t> banks{0};
};
struct IndexLease
{
    id<MTLResource> native=nil;
    std::shared_ptr<Accounting> accounting;
    std::uint64_t bytes=0;
    bool index_role=true;
    ~IndexLease()
    {
        native=nil;
        if(accounting)(index_role?accounting->index_bytes:accounting->binding_bytes).fetch_sub(bytes,std::memory_order_relaxed);
    }
};
struct ResourceKey
{
    void* allocation=nullptr;
    bool index_role=true;
    bool operator==(const ResourceKey&)const=default;
};
struct ResourceKeyHash
{std::size_t operator()(const ResourceKey& key)const noexcept{return std::hash<void*>{}(key.allocation)^(std::size_t(key.index_role)<<1);}};
using ResourceRegistry=std::unordered_map<ResourceKey,std::weak_ptr<IndexLease>,ResourceKeyHash>;
struct Key
{
    std::uint64_t identity,flavor,revision,index_address,index_offset;
    std::uint32_t row,generation,index_count,index_type;
    bool operator==(const Key&)const=default;
};
struct Bank
{
    id<MTLIndirectCommandBuffer> native=nil;
    std::vector<Key> keys;
    // Generic banks retain their exact key values. Cohort banks instead use
    // one private immutable membership identity; neither cache form owns a
    // CPU scene packet or a real page-reader token.
    std::weak_ptr<const ALMetalWorldSourceCommandMembership> membership;
    const ALMetalWorldSourceCommandMembership* membership_pointer=nullptr;
    std::size_t count=0;
    std::shared_ptr<const ALMetalWorldSourceDirectSlot> direct_slot;
    bool direct_normal_hdr=false;
    std::uint64_t binding_limit=0;
    std::vector<std::shared_ptr<const ALMetalWorldResourceTable>> direct_tables;
    std::vector<std::shared_ptr<IndexLease>> indices;
    std::shared_ptr<Accounting> accounting;
    std::uint64_t bytes=0,hash=0;
    ~Bank()
    {
        if(accounting)
        {
            native=nil;direct_tables.clear();direct_slot.reset();indices.clear();
            accounting->icb_bytes.fetch_sub(bytes,std::memory_order_relaxed);
            accounting->banks.fetch_sub(1,std::memory_order_relaxed);
        }
    }
};
struct BankPublication
{
    std::list<std::shared_ptr<const Bank>> addition;
    ResourceRegistry indices;
    std::size_t evictions=0;
};
bool retainPublication(ALMetalCommandBuffer& command,
    const std::shared_ptr<const ALMetalWorldSourceCommands::Batch>& batch,
    std::shared_ptr<const void> credit,std::string* error)
{
    if(!credit)return command.retainUntilCompletion(batch,error);
    struct Owner
    {
        std::shared_ptr<const ALMetalWorldSourceCommands::Batch> batch;
        std::shared_ptr<const void> credit;
    };
    // One fallible retention, before cache commit. A partial pair of separate
    // retains could strand an unpublishable Bank on the command over its cap.
    return command.retainUntilCompletion(std::make_shared<Owner>(Owner{batch,std::move(credit)}),error);
}
std::uint64_t hashKey(std::uint64_t hash,const Key& key)
{
    const std::uint64_t words[]{key.identity,key.flavor,key.revision,key.index_address,
        key.index_offset,key.row,key.generation,key.index_count,key.index_type};
    for(auto word:words){hash^=word;hash*=1099511628211ULL;}return hash;
}
}
struct ALMetalWorldSourceCommands::Impl
{
    id<MTLDevice> device=nil;
    bool supported=false,force_failure=false,preparing=false;
    std::optional<std::size_t> execution_failure_after;
    std::uint64_t epoch=1;
    std::uint64_t binding_limit=0;
    ALMetalWorldSourceCommandLimits limits;
    ALMetalWorldSourceCommandStats statistics;
    std::shared_ptr<ALMetalWorldSourcePages::PublicationAdmission> admission;
    std::shared_ptr<Accounting> accounting=std::make_shared<Accounting>();
    std::list<std::shared_ptr<const Bank>> cache;
    ResourceRegistry indices;
    void fail(ALMetalWorldSourceCommandFailure reason,ALMetalWorldSourceCommandFailure* out,
              std::string* error,const char* message)
    {
        ++statistics.failures[static_cast<std::size_t>(reason)];statistics.last_failure=reason;
        if(out)*out=reason;if(error)*error=message;
    }
    bool overCapacity()const
    {
        const auto icb=accounting->icb_bytes.load(std::memory_order_relaxed);
        const auto index=accounting->index_bytes.load(std::memory_order_relaxed);
        const auto bindings=accounting->binding_bytes.load(std::memory_order_relaxed);
        return icb>limits.maximum_live_native_bytes || index>limits.maximum_live_native_bytes-icb ||
            bindings>binding_limit ||
            accounting->banks.load(std::memory_order_relaxed)>limits.maximum_live_banks;
    }
    bool planPublication(const std::shared_ptr<const Bank>& bank,BankPublication& plan)
    {
        // Stage the complete weak registry and cache node before touching the
        // usable cache. Allocation/admission/encoding/retention failures may
        // therefore leave every prior warm bank available (PPD207).
        plan.indices=indices;
        std::erase_if(plan.indices,[](const auto& entry){return entry.second.expired();});
        for(const auto& lease:bank->indices)plan.indices[{(__bridge void*)lease->native,lease->index_role}]=lease;
        auto live=accounting->banks.load(std::memory_order_relaxed);
        auto icb=accounting->icb_bytes.load(std::memory_order_relaxed);
        auto index_bytes=accounting->index_bytes.load(std::memory_order_relaxed);
        auto binding_bytes=accounting->binding_bytes.load(std::memory_order_relaxed);
        const auto asset_limit=bank->direct_slot?bank->binding_limit:binding_limit;
        auto cached=cache.size()+std::size_t(limits.maximum_cached_banks!=0);
        const auto fits=[&]
        {
            return live<=limits.maximum_live_banks&&icb<=limits.maximum_live_native_bytes&&
                index_bytes<=limits.maximum_live_native_bytes-icb&&
                binding_bytes<=asset_limit&&
                cached<=limits.maximum_cached_banks&&
                std::count_if(plan.indices.begin(),plan.indices.end(),[](const auto& item){auto lease=item.second.lock();return lease&&lease->index_role;})<=limits.maximum_unique_index_buffers;
        };
        std::unordered_map<const IndexLease*,std::size_t> removed_index_references;
        for(auto it=cache.rbegin();!fits()&&it!=cache.rend();++it)
        {
            ++plan.evictions;--cached;
            // Retired/queued banks remain counted after cache eviction. Only
            // a cache's sole Bank owner can release its native allocations.
            // Completion can decrease references concurrently, never add a
            // new cache reader on this owner-only preparation boundary.
            if(it->use_count()!=1)continue;
            const auto& victim=**it;--live;icb-=victim.bytes;
            for(const auto& lease:victim.indices)
                if(++removed_index_references[lease.get()]==static_cast<std::size_t>(lease.use_count()))
                {(lease->index_role?index_bytes:binding_bytes)-=lease->bytes;plan.indices.erase({(__bridge void*)lease->native,lease->index_role});}
        }
        if(!fits())return false;
        if(limits.maximum_cached_banks)plan.addition.push_front(bank);
        return true;
    }
    void commitPublication(BankPublication& plan) noexcept
    {
        indices.swap(plan.indices);
        cache.splice(cache.begin(),plan.addition);
        for(std::size_t count=0;count<plan.evictions;++count)cache.pop_back();
        statistics.evictions+=plan.evictions;++statistics.builds;
    }
};
struct ALMetalWorldSourceCommands::Batch::Impl
{
    std::shared_ptr<const Bank> bank;
    std::shared_ptr<const ALMetalWorldSourceFrameView> frame;
    id<MTLCommandBuffer> command=nil;
    id<MTLDevice> device=nil;
};
ALMetalWorldSourceCommands::Batch::Batch(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceCommands::Batch::~Batch()=default;
std::size_t ALMetalWorldSourceCommands::Batch::size()const noexcept{return mImpl->bank->count;}
ALMetalWorldSourceCommands::ALMetalWorldSourceCommands(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldSourceCommands::~ALMetalWorldSourceCommands()=default;
std::shared_ptr<ALMetalWorldSourceCommands> ALMetalWorldSourceCommands::create(
    const ALMetalDevice& device,ALMetalWorldSourceCommandLimits limits,std::string* error)
{
    if(!device.nativeHandle()){if(error)*error="Source command banks require a native device";return {};}
    auto impl=std::make_unique<Impl>();impl->device=(__bridge id<MTLDevice>)device.nativeHandle();impl->limits=limits;
    impl->supported=[impl->device supportsFamily:MTLGPUFamilyApple7];
    return std::shared_ptr<ALMetalWorldSourceCommands>(new ALMetalWorldSourceCommands(std::move(impl)));
}
bool ALMetalWorldSourceCommands::supported()const noexcept{return mImpl->supported;}
void ALMetalWorldSourceCommands::setPublicationAdmission(ALMetalWorldSourcePages::PublicationAdmission admission)
{
    auto holder=admission
        ?std::make_shared<ALMetalWorldSourcePages::PublicationAdmission>(std::move(admission)):nullptr;
    mImpl->admission=std::move(holder);++mImpl->epoch;
}
void ALMetalWorldSourceCommands::clear()noexcept
{++mImpl->epoch;mImpl->cache.clear();std::erase_if(mImpl->indices,[](const auto& value){return value.second.expired();});}
void ALMetalWorldSourceCommands::trimUnusedBanks()noexcept
{
    for(auto it=mImpl->cache.begin();it!=mImpl->cache.end();)
        if((*it)->membership_pointer&&(*it)->membership.expired()){it=mImpl->cache.erase(it);++mImpl->statistics.evictions;}
        else ++it;
    std::erase_if(mImpl->indices,[](const auto& value){return value.second.expired();});
}
void ALMetalWorldSourceCommands::failNextPublicationForTesting()noexcept{mImpl->force_failure=true;}
void ALMetalWorldSourceCommands::failExecutionAfterForTesting(std::size_t successful_ranges)noexcept
{mImpl->execution_failure_after=successful_ranges;}
ALMetalWorldSourceCommandStats ALMetalWorldSourceCommands::stats()const noexcept
{
    auto result=mImpl->statistics;result.cached_banks=mImpl->cache.size();
    result.live_banks=mImpl->accounting->banks.load(std::memory_order_relaxed);
    result.live_icb_allocated_bytes=mImpl->accounting->icb_bytes.load(std::memory_order_relaxed);
    result.unique_retained_index_bytes=mImpl->accounting->index_bytes.load(std::memory_order_relaxed);
    result.unique_retained_binding_bytes=mImpl->accounting->binding_bytes.load(std::memory_order_relaxed);
    result.retained_binding_limit_bytes=mImpl->binding_limit;return result;
}
std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareForCommand(
    ALMetalCommandBuffer& command,std::shared_ptr<const ALMetalWorldSourceFrameView> frame,
    std::span<const ALMetalWorldSourceCommandDraw> draws,std::string* error,ALMetalWorldSourceCommandFailure* failure)
try
{
    auto& state=*mImpl;++state.statistics.requests;if(failure)*failure=ALMetalWorldSourceCommandFailure::none;
    const auto reject=[&](auto reason,const char* message)->std::shared_ptr<const Batch>{state.fail(reason,failure,error,message);return {};};
    if(state.preparing)return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command preparation is reentrant");
    struct PreparationGuard{bool& active;~PreparationGuard(){active=false;}} guard{state.preparing};
    state.preparing=true;
    if(!state.supported)return reject(ALMetalWorldSourceCommandFailure::unsupported,"Native source command banks are unsupported");
    if(command.isCommitted()||command.nativeDevice()!=(__bridge void*)state.device||!frame||frame->directBindings()||draws.empty()||draws.size()>alMetalWorldSourceMaximumRows)
        return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command bank input is invalid");
    std::vector<Key> keys;keys.reserve(draws.size());std::uint64_t hash=1469598103934665603ULL;
    for(const auto& draw:draws)
    {
        if(!draw.key.identity||!draw.key.revision||draw.view_index>=frame->size()||!draw.indices||!draw.index_count)
            return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command source/index is missing");
        if(!frame->matchesSourceCommand(draw.view_index,draw.key,*draw.indices))
            return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command does not match its frozen row/index generation");
        void* indices=draw.indices->nativeHandle();
        const std::size_t stride=draw.index_type==ALMetalIndexType::uint16?2:4;
        const auto length=draw.indices->length();
        // The private FrameView proof already validated the exact device and
        // immutable native allocation. Cached Banks natively retain that
        // allocation, so its identity cannot recycle while it is a key. Warm
        // hits need no per-draw Objective-C device/GPU-address queries.
        if(!indices||(draw.index_type!=ALMetalIndexType::uint16&&draw.index_type!=ALMetalIndexType::uint32)||
            draw.index_offset%stride||draw.index_offset>length||draw.index_count>(length-draw.index_offset)/stride)
            return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command index range is invalid");
        Key key{draw.key.identity,draw.key.flavor,draw.key.revision,
            reinterpret_cast<std::uintptr_t>(indices),draw.index_offset,
            frame->rowId(draw.view_index),frame->allocationGeneration(draw.view_index),draw.index_count,
            static_cast<std::uint32_t>(draw.index_type)};
        if(key.row>=alMetalWorldSourceMaximumRows||!key.generation)
            return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command row generation is invalid");
        hash=hashKey(hash,key);keys.push_back(key);
    }
    std::shared_ptr<const Bank> selected;
    std::optional<BankPublication> publication;
    std::shared_ptr<const void> publication_credit;
    std::uint64_t charged_arguments=0;
    for(auto it=state.cache.begin();it!=state.cache.end();++it)
        if(!(*it)->membership_pointer&&(*it)->hash==hash&&(*it)->keys==keys)
        {selected=*it;state.cache.splice(state.cache.begin(),state.cache,it);++state.statistics.hits;break;}
    if(!selected)
    {
        if(std::exchange(state.force_failure,false))return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Forced native source command publication failure");
        auto bank=std::make_shared<Bank>();bank->hash=hash;bank->keys=std::move(keys);bank->count=bank->keys.size();
        auto descriptor=[MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=YES;
        descriptor.maxVertexBufferBindCount=0;descriptor.maxFragmentBufferBindCount=0;
        bank->native=[state.device newIndirectCommandBufferWithDescriptor:descriptor maxCommandCount:draws.size() options:MTLResourceStorageModeShared];
        if(!bank->native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Could not allocate a native source command bank");
        bank->bytes=bank->native.allocatedSize;bank->accounting=state.accounting;
        state.accounting->icb_bytes.fetch_add(bank->bytes,std::memory_order_relaxed);
        state.accounting->banks.fetch_add(1,std::memory_order_relaxed);
        for(const auto& draw:draws)
        {
            void* identity=draw.indices->nativeHandle();std::shared_ptr<IndexLease> lease;
            if(const auto prior=std::find_if(bank->indices.begin(),bank->indices.end(),
                    [&](const auto& value){return value->index_role&&(__bridge void*)value->native==identity;});prior!=bank->indices.end())lease=*prior;
            else if(const auto found=state.indices.find({identity,true});found!=state.indices.end())lease=found->second.lock();
            if(!lease)
            {
                lease=std::make_shared<IndexLease>();lease->native=(__bridge id<MTLBuffer>)identity;
                // PPD188: defend the native helper's cold boundary as well
                // as the FrameView publisher's unique-resource device proof.
                if(lease->native.device!=state.device)
                    return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command index belongs to another device");
                lease->bytes=lease->native.allocatedSize;lease->accounting=state.accounting;
                state.accounting->index_bytes.fetch_add(lease->bytes,std::memory_order_relaxed);
            }
            if(std::find(bank->indices.begin(),bank->indices.end(),lease)==bank->indices.end())bank->indices.push_back(std::move(lease));
        }
        if(state.admission)
        {
            const auto epoch=state.epoch;
            // Retain the exact callable holder so mutable deferral age persists
            // and replacing it reentrantly cannot destroy the active callable.
            auto admission=state.admission;
            ALMetalWorldSourcePublicationWork work;
            // Canonical logical indexed-call arguments, not driver traffic or
            // an MSL ABI: six 32-bit primitive/count/type/instance/base fields
            // plus two 64-bit native index identity/byte-offset fields.
            work.encoded_bank_range_bytes=draws.size()*(6*sizeof(std::uint32_t)+2*sizeof(std::uint64_t));
            work.allocation_bytes=bank->bytes;++state.statistics.admission_requests;
            publication_credit=(*admission)(work);
            if(epoch!=state.epoch||command.isCommitted())return reject(ALMetalWorldSourceCommandFailure::admission,"Native source command publication changed during admission");
            if(!publication_credit){++state.statistics.admission_denials;return reject(ALMetalWorldSourceCommandFailure::admission,"Native source command publication was deferred");}
            charged_arguments=work.encoded_bank_range_bytes;
        }
        // PPD187: admission may reenter caller-owned storage. Encode only the
        // validated snapshot whose native allocations the Bank already pins.
        for(std::size_t i=0;i<bank->keys.size();++i)
        {
            id<MTLIndirectRenderCommand> native=[bank->native indirectRenderCommandAtIndex:i];
            if(!native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Native source indirect command proxy is unavailable");
            const auto& draw=bank->keys[i];
            [native drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.index_count
                indexType:draw.index_type==static_cast<std::uint32_t>(ALMetalIndexType::uint16)?MTLIndexTypeUInt16:MTLIndexTypeUInt32
                indexBuffer:(__bridge id<MTLBuffer>)reinterpret_cast<void*>(static_cast<std::uintptr_t>(draw.index_address))
                indexBufferOffset:draw.index_offset instanceCount:1 baseVertex:0 baseInstance:draw.row];
        }
        selected=std::move(bank);publication.emplace();
        if(!state.planPublication(selected,*publication))
            return reject(ALMetalWorldSourceCommandFailure::capacity,"Native source command live/retired storage is full");
    }
    auto impl=std::make_unique<Batch::Impl>();impl->bank=std::move(selected);impl->frame=std::move(frame);
    impl->command=(__bridge id<MTLCommandBuffer>)command.nativeHandle();impl->device=state.device;
    auto result=std::shared_ptr<const Batch>(new Batch(std::move(impl)));
    if(!retainPublication(command,result,std::move(publication_credit),error))return reject(ALMetalWorldSourceCommandFailure::retention,"Native source command lease retention failed");
    if(publication){state.commitPublication(*publication);state.statistics.charged_logical_argument_bytes+=charged_arguments;}
    state.statistics.last_failure=ALMetalWorldSourceCommandFailure::none;return result;
}
catch(const std::bad_alloc&)
{mImpl->fail(ALMetalWorldSourceCommandFailure::host_allocation,failure,nullptr,"");if(error)error->clear();return {};}
catch(...)
{mImpl->fail(ALMetalWorldSourceCommandFailure::admission,failure,error,"Native source command publication callback failed");return {};}

std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareCohortForCommand(
    ALMetalCommandBuffer& command,std::shared_ptr<const ALMetalWorldSourceFrameView> frame,
    std::string* error,ALMetalWorldSourceCommandFailure* failure)
try
{
    auto& state=*mImpl;++state.statistics.requests;if(failure)*failure=ALMetalWorldSourceCommandFailure::none;
    const auto reject=[&](auto reason,const char* message)->std::shared_ptr<const Batch>{state.fail(reason,failure,error,message);return {};};
    if(state.preparing)return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command preparation is reentrant");
    struct PreparationGuard{bool& active;~PreparationGuard(){active=false;}} guard{state.preparing};
    state.preparing=true;
    if(!state.supported)return reject(ALMetalWorldSourceCommandFailure::unsupported,"Native source command banks are unsupported");
    const auto membership=frame?frame->commandMembership():nullptr;
    if(command.isCommitted()||command.nativeDevice()!=(__bridge void*)state.device||!membership||
       membership->nativeDevice()!=(__bridge void*)state.device||membership->calls().empty()||
       membership->calls().size()>alMetalWorldSourceMaximumRows||frame->size()!=membership->calls().size()||!frame->matchesDirectCommand(command))
        return reject(ALMetalWorldSourceCommandFailure::validation,"Native cohort command membership or device is invalid");
    const bool direct=frame->directBindings();const auto slot=frame->directBindingSlot();
    const auto bindings=membership->directBindings();
    if(direct&&(!slot||!slot->nativeBuffer()||bindings.size()!=membership->calls().size()||!membership->directPolicyBuffer()))
        return reject(ALMetalWorldSourceCommandFailure::validation,"Native direct command binding membership is incomplete");

    std::shared_ptr<const Bank> selected;
    std::optional<BankPublication> publication;
    std::shared_ptr<const void> publication_credit;
    std::uint64_t charged_arguments=0;
    std::uint64_t charged_bindings=0;
    for(auto it=state.cache.begin();it!=state.cache.end();)
    {
        const auto& bank=*it;
        if(bank->membership_pointer&&bank->membership.expired())
        {it=state.cache.erase(it);++state.statistics.evictions;continue;}
        // A current strong membership makes an equal weak control block live.
        // Pointer AND control-block identity prevent metadata-address reuse
        // from certifying a different generation. No per-source scan occurs.
        if(bank->membership_pointer==membership.get()&&!bank->membership.owner_before(membership)&&
           !membership.owner_before(bank->membership)&&bank->direct_slot==slot&&
           (!direct||bank->direct_normal_hdr==frame->directNormalHDR()))
        {selected=bank;state.cache.splice(state.cache.begin(),state.cache,it);++state.statistics.hits;if(direct)++state.statistics.direct_hits;break;}
        ++it;
    }
    if(!selected)
    {
        if(std::exchange(state.force_failure,false))return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Forced native source command publication failure");
        auto bank=std::make_shared<Bank>();bank->membership=membership;bank->membership_pointer=membership.get();
        bank->direct_slot=slot;bank->direct_normal_hdr=frame->directNormalHDR();
        bank->binding_limit=direct?frame->directBindingAssetLimit():0;
        const auto calls=membership->calls();bank->count=calls.size();
        auto descriptor=[MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=!direct;
        descriptor.maxVertexBufferBindCount=direct?12:0;descriptor.maxFragmentBufferBindCount=direct?2:0;
        bank->native=[state.device newIndirectCommandBufferWithDescriptor:descriptor maxCommandCount:bank->count options:MTLResourceStorageModeShared];
        if(!bank->native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Could not allocate a native cohort command bank");
        bank->bytes=bank->native.allocatedSize;bank->accounting=state.accounting;
        state.accounting->icb_bytes.fetch_add(bank->bytes,std::memory_order_relaxed);
        state.accounting->banks.fetch_add(1,std::memory_order_relaxed);
        for(const auto& call:calls)
        {
            const auto stride=call.index_type==ALMetalIndexType::uint16?2u:4u;
            id<MTLBuffer> indices=(__bridge id<MTLBuffer>)call.native_indices;
            if(!indices||!call.key.identity||!call.key.revision||!call.index_count||
               (call.index_type!=ALMetalIndexType::uint16&&call.index_type!=ALMetalIndexType::uint32)||
               call.index_offset%stride||call.index_offset>indices.length||
               call.index_count>(indices.length-call.index_offset)/stride||
               call.row_id>=alMetalWorldSourceMaximumRows||!call.row_generation)
                return reject(ALMetalWorldSourceCommandFailure::validation,"Native cohort indexed call is invalid");
            std::shared_ptr<IndexLease> lease;
            if(const auto prior=std::find_if(bank->indices.begin(),bank->indices.end(),
                    [&](const auto& value){return value->index_role&&(__bridge void*)value->native==call.native_indices;});prior!=bank->indices.end())lease=*prior;
            else if(const auto found=state.indices.find({call.native_indices,true});found!=state.indices.end())lease=found->second.lock();
            if(!lease)
            {
                lease=std::make_shared<IndexLease>();lease->native=indices;
                if(lease->native.device!=state.device)
                    return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command index belongs to another device");
                lease->bytes=lease->native.allocatedSize;lease->accounting=state.accounting;
                state.accounting->index_bytes.fetch_add(lease->bytes,std::memory_order_relaxed);
            }
            if(std::find(bank->indices.begin(),bank->indices.end(),lease)==bank->indices.end())bank->indices.push_back(std::move(lease));
        }
        if(direct)
        {
            std::unordered_map<void*,std::shared_ptr<IndexLease>> unique;
            unique.reserve(bank->indices.size()+bindings.size()*4);
            const auto retain=[&](void* identity,std::uint64_t expected=0)
            {
                if(!identity)return false;
                if(unique.contains(identity))return true;
                std::shared_ptr<IndexLease> lease;
                if(auto found=state.indices.find({identity,false});found!=state.indices.end())lease=found->second.lock();
                if(!lease)
                {
                    lease=std::make_shared<IndexLease>();lease->native=(__bridge id<MTLResource>)identity;lease->index_role=false;
                    if(lease->native.device!=state.device)return false;
                    lease->bytes=lease->native.allocatedSize;
                    if(!lease->bytes||(expected&&lease->bytes!=expected))return false;
                    lease->accounting=state.accounting;state.accounting->binding_bytes.fetch_add(lease->bytes,std::memory_order_relaxed);
                }
                if(expected&&lease->bytes!=expected)return false;
                unique.emplace(identity,lease);bank->indices.push_back(std::move(lease));return true;
            };
            if(!retain(slot->nativeBuffer())||!retain(membership->directPolicyBuffer()))
                return reject(ALMetalWorldSourceCommandFailure::validation,"Native direct command view/policy ownership is invalid");
            for(const auto& binding:bindings)
            {
                for(auto geometry:binding.geometry)if(!retain(geometry))return reject(ALMetalWorldSourceCommandFailure::validation,"Native direct geometry ownership is invalid");
                void* numeric=binding.legacy_numeric?binding.legacy_numeric:(frame->directNormalHDR()?binding.pbr_hdr:binding.pbr_ldr);
                if(!retain(numeric)||!retain(binding.resource_table))return reject(ALMetalWorldSourceCommandFailure::validation,"Native direct material ownership is invalid");
            }
            for(const auto& asset:membership->directAssets())if(!retain(asset.allocation,asset.bytes))
                return reject(ALMetalWorldSourceCommandFailure::validation,"Native direct texture footprint is invalid");
            const auto tables=frame->directBindingTables();bank->direct_tables.assign(tables.begin(),tables.end());
        }
        if(state.admission)
        {
            const auto epoch=state.epoch;auto admission=state.admission;
            ALMetalWorldSourcePublicationWork work;
            const auto argument_bytes=bank->count*(6*sizeof(std::uint32_t)+2*sizeof(std::uint64_t));
            std::uint64_t binding_bytes=0;
            if(direct)for(const auto& binding:bindings)
                binding_bytes+=(binding.legacy_numeric?12u:13u)*(2*sizeof(std::uint64_t));
            work.encoded_bank_range_bytes=argument_bytes+binding_bytes;
            work.allocation_bytes=bank->bytes;++state.statistics.admission_requests;
            publication_credit=(*admission)(work);
            if(epoch!=state.epoch||command.isCommitted()||command.nativeDevice()!=(__bridge void*)state.device)
                return reject(ALMetalWorldSourceCommandFailure::admission,"Native cohort command publication changed during admission");
            if(!publication_credit){++state.statistics.admission_denials;return reject(ALMetalWorldSourceCommandFailure::admission,"Native source command publication was deferred");}
            charged_arguments=argument_bytes;charged_bindings=binding_bytes;
        }
        // Membership is privately immutable numeric storage, kept alive by
        // this local lease through callbacks. It contains no borrowed Draw
        // span that a reentrant producer could replace (PPD187).
        for(std::size_t index=0;index<calls.size();++index)
        {
            id<MTLIndirectRenderCommand> native=[bank->native indirectRenderCommandAtIndex:index];
            if(!native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Native source indirect command proxy is unavailable");
            const auto& call=calls[index];
            if(direct)
            {
                const auto& binding=bindings[index];constexpr unsigned slots[]{0,1,2,3,8,9,10};
                for(unsigned attribute=0;attribute<7;++attribute)
                    [native setVertexBuffer:(__bridge id<MTLBuffer>)binding.geometry[attribute] offset:binding.offsets[attribute] atIndex:slots[attribute]];
                id<MTLBuffer> view=(__bridge id<MTLBuffer>)slot->nativeBuffer();
                [native setVertexBuffer:view offset:slot->regionOffset(binding.region) atIndex:4];
                [native setVertexBuffer:view offset:slot->globalsOffset() atIndex:5];
                [native setVertexBuffer:(__bridge id<MTLBuffer>)membership->directPolicyBuffer() offset:binding.policy_offset atIndex:6];
                void* numeric=binding.legacy_numeric?binding.legacy_numeric:(bank->direct_normal_hdr?binding.pbr_hdr:binding.pbr_ldr);
                if(!binding.legacy_numeric)[native setVertexBuffer:(__bridge id<MTLBuffer>)numeric offset:0 atIndex:11];
                [native setFragmentBuffer:(__bridge id<MTLBuffer>)numeric offset:0 atIndex:0];
                [native setFragmentBuffer:(__bridge id<MTLBuffer>)binding.resource_table offset:0 atIndex:1];
            }
            [native drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:call.index_count
                indexType:call.index_type==ALMetalIndexType::uint16?MTLIndexTypeUInt16:MTLIndexTypeUInt32
                indexBuffer:(__bridge id<MTLBuffer>)call.native_indices indexBufferOffset:call.index_offset
                instanceCount:1 baseVertex:0 baseInstance:call.row_id];
        }
        selected=std::move(bank);publication.emplace();
        if(!state.planPublication(selected,*publication))
            return reject(ALMetalWorldSourceCommandFailure::capacity,"Native source command live/retired storage is full");
    }
    auto impl=std::make_unique<Batch::Impl>();impl->bank=std::move(selected);impl->frame=std::move(frame);
    impl->command=(__bridge id<MTLCommandBuffer>)command.nativeHandle();impl->device=state.device;
    auto result=std::shared_ptr<const Batch>(new Batch(std::move(impl)));
    if(!retainPublication(command,result,std::move(publication_credit),error))return reject(ALMetalWorldSourceCommandFailure::retention,"Native source command lease retention failed");
    if(publication){state.commitPublication(*publication);state.statistics.charged_logical_argument_bytes+=charged_arguments;
        state.statistics.charged_logical_binding_bytes+=charged_bindings;
        if(direct)state.binding_limit=result->mImpl->frame->directBindingAssetLimit();
        if(direct){++state.statistics.direct_builds;state.statistics.direct_encoded_calls+=result->size();}}
    state.statistics.last_failure=ALMetalWorldSourceCommandFailure::none;return result;
}
catch(const std::bad_alloc&)
{mImpl->fail(ALMetalWorldSourceCommandFailure::host_allocation,failure,nullptr,"");if(error)error->clear();return {};}
catch(...)
{mImpl->fail(ALMetalWorldSourceCommandFailure::admission,failure,error,"Native source command publication callback failed");return {};}

bool ALMetalWorldSourceCommands::execute(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,const Batch& batch,std::string* error)
{return executeRange(encoder,command,batch,0,batch.size(),error);}

bool ALMetalWorldSourceCommands::executeRange(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const Batch& batch,std::size_t first,std::size_t count,std::string* error)
{
    const auto& prepared=*batch.mImpl;
    if(!count||first>prepared.bank->count||count>prepared.bank->count-first||
        !encoder.canExecuteIndirect()||encoder.nativeDeviceForIndirect()!=(__bridge void*)prepared.device||
        encoder.nativeCommandBufferForIndirect()!=(__bridge void*)prepared.command||command.nativeHandle()!=(__bridge void*)prepared.command||command.isCommitted())
    {mImpl->fail(ALMetalWorldSourceCommandFailure::execution,nullptr,error,"Native source bank requires its prepared command and compatible pipeline");return false;}
    if(mImpl->execution_failure_after&&*mImpl->execution_failure_after==0)
    {
        mImpl->execution_failure_after.reset();
        mImpl->fail(ALMetalWorldSourceCommandFailure::execution,nullptr,error,"Forced native source range execution failure");
        return false;
    }
    if(!prepared.frame->bind(encoder,command,error))return false;
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForIndirect();
    if(!native)return false;
    [native useResource:prepared.bank->native usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
    // matchesSourceCommand proved each index belongs to this FrameView;
    // frame.bind declared its exact geometry/index union once per encoder.
    [native executeCommandsInBuffer:prepared.bank->native withRange:NSMakeRange(first,count)];
    encoder.recordIndirectSubmission(static_cast<std::uint32_t>(count));
    if(mImpl->execution_failure_after)--*mImpl->execution_failure_after;
    // Direct banks own their buffer binds. Subsequent ordinary bridge calls
    // must restore their slots even if its prior cached pointer was equal.
    if(prepared.bank->direct_slot)encoder.invalidateIndirectBufferBindings();
    ++mImpl->statistics.executions;mImpl->statistics.logical_draws+=count;return true;
}
