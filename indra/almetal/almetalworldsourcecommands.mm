/**
 * @file almetalworldsourcecommands.mm
 * @brief Persistent indexed ICBs with bounded native-only ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldsourcecommands.h"
#include "almetalworld.h"
#include "almetalworldanimated.h"
#include "almetalworldvisibility.h"
#include <cstddef>
#include <cstring>
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
namespace RP=ALMetalResourcePlanning;

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
struct AnimatedSlot
{
    std::vector<std::shared_ptr<const ALMetalWorldAnimatedGpuSource>> sources;
    std::shared_ptr<ALMetalBuffer> views,root,visible;
    std::weak_ptr<const void> reader;
    std::size_t stride=0;
    void* library=nullptr;
    bool normal_hdr=false,gpu_visibility=true;
};
// Exact MSL almetal_world_static_visibility_commands row ABI. Index pointers
// already include the actual indexed-call byte offset; the other type is null.
struct alignas(16) StaticVisibilityRow
{
    std::array<float,4> minimum{},maximum{};
    std::uint64_t index16=0,index32=0;
    std::array<std::uint32_t,4> call{};
};
static_assert(sizeof(StaticVisibilityRow)==64 && alignof(StaticVisibilityRow)==16);
static_assert(offsetof(StaticVisibilityRow,index16)==32 && offsetof(StaticVisibilityRow,call)==48);
struct Bank
{
    id<MTLIndirectCommandBuffer> native=nil;
    RP::View plan_view;
    std::vector<Key> keys;
    // Generic banks retain their exact key values. Cohort banks instead use
    // one private immutable membership identity; neither cache form owns a
    // CPU scene packet or a real page-reader token.
    std::weak_ptr<const ALMetalWorldSourceCommandMembership> membership;
    const ALMetalWorldSourceCommandMembership* membership_pointer=nullptr;
    std::size_t count=0;
    std::shared_ptr<AnimatedSlot> animated;
    std::shared_ptr<const ALMetalWorldSourceDirectSlot> direct_slot;
    bool direct_normal_hdr=false;
    id<MTLLibrary> static_visibility_library=nil;
    std::shared_ptr<ALMetalBuffer> static_visibility_rows,static_visibility_root;
    std::vector<void*> static_visibility_indices;
    std::size_t static_visibility_qualified_count=0;
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
    id<MTLLibrary> animated_library=nil;
    id<MTLArgumentEncoder> pose_encoder=nil,root_encoder=nil;
    std::unique_ptr<ALMetalComputePipeline> animated_pipeline;
    id<MTLLibrary> static_visibility_library=nil;
    id<MTLArgumentEncoder> static_visibility_root_encoder=nil;
    std::unique_ptr<ALMetalComputePipeline> static_visibility_pipeline;
    bool supported=false,force_failure=false,preparing=false,fail_visibility_after_begin=false;
    const ALMetalDevice* wrapper=nullptr;
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
        const auto asset_limit=(bank->direct_slot||bank->animated)?bank->binding_limit:binding_limit;
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
    std::shared_ptr<const void> animated_reader;
    std::weak_ptr<const void> declared_encoder_identity;
    std::vector<std::shared_ptr<const void>> animated_native;
    std::vector<ALMetalWorldDataSlice> animated_palettes;
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
    auto impl=std::make_unique<Impl>();impl->device=(__bridge id<MTLDevice>)device.nativeHandle();impl->wrapper=&device;impl->limits=limits;
    impl->supported=[impl->device supportsFamily:MTLGPUFamilyApple7];
    return std::shared_ptr<ALMetalWorldSourceCommands>(new ALMetalWorldSourceCommands(std::move(impl)));
}
bool ALMetalWorldSourceCommands::supported()const noexcept{return mImpl->supported;}

ALMetalWorldSourceCommandDecision ALMetalWorldSourceCommands::applicability(
    const ALMetalWorldSourceCohortView& view,const ALMetalWorldSourceCommandContext& context,
    ALMetalWorldSourceCommandPolicy policy)const noexcept
{
    using Route=ALMetalWorldSourceCommandRoute;using Reason=ALMetalWorldSourceCommandReason;
    ALMetalWorldSourceCommandDecision result;
    if(context.wireframe||context.depth_clamp||context.opaque_coverage_stamp)
    {result.reason=Reason::raster_policy;return result;}
    const auto work=alMetalInspectWorldCohortVisibilityWork(view);
    result.draws=work.draws;result.indexed_elements=work.indexed_elements;
    result.qualified_indexed_elements=work.qualified_indexed_elements;
    if(!work.valid||!context.ordered_ranges)return result;
    if(work.coarse==ALMetalWorldCpuVisibilityResult::inside)
    {result.reason=Reason::inside;return result;}
    if(work.coarse==ALMetalWorldCpuVisibilityResult::outside)
    {
        // A cheap coarse proof already exists. The caller's ordered direct
        // path keeps unknown rows and query boundaries; no GPU setup needed.
        result.route=Route::cpu_visibility;result.reason=Reason::outside;return result;
    }
    if(work.coarse==ALMetalWorldCpuVisibilityResult::unknown||!work.qualified_draws)
    {result.reason=Reason::unknown_bounds;return result;}
    if(context.ordered_ranges>policy.maximum_ordered_ranges)
    {result.reason=Reason::fragmented;return result;}
    if(work.qualified_indexed_elements<policy.minimum_qualified_indices||
       work.outside_center_indexed_elements<policy.minimum_qualified_indices/4)
    {result.reason=Reason::small_work;return result;}
    if(!supported()||!context.pipeline_supports_indirect)
    {result.reason=Reason::unsupported;return result;}
    result.route=Route::gpu_visibility;result.reason=Reason::heavy_partial;return result;
}

ALMetalWorldSourceCommandDecision ALMetalWorldSourceCommands::applicability(
    std::span<const ALMetalWorldSourceReference* const> sources,
    const ALMetalWorldSourceCommandContext& context,ALMetalWorldSourceCommandPolicy policy)const noexcept
{
    using Route=ALMetalWorldSourceCommandRoute;using Reason=ALMetalWorldSourceCommandReason;
    ALMetalWorldSourceCommandDecision result;result.draws=sources.size();
    if(sources.empty()||sources.size()>alMetalWorldSourceMaximumRows||!context.ordered_ranges)return result;
    if(context.wireframe||context.depth_clamp||context.opaque_coverage_stamp)
    {result.reason=Reason::raster_policy;return result;}
    for(const auto* reference:sources)
    {
        if(!reference||!reference->animated_source||!reference->animated_palette||!reference->source||
           reference->source!=&reference->animated_source->prototype()||
           !ALMetalWorldSourceCohort::validView(reference->view)||
           std::memcmp(reference->view.texture_matrix,reference->source->texture_matrix,sizeof(reference->view.texture_matrix)))
            return result;
        const auto count=reference->source->index_count;
        if(!count)return result;
        result.indexed_elements+=count;
        if(reference->animated_source->gpuReady()&&reference->animated_source->boundsQualified())
            result.qualified_indexed_elements+=count;
    }
    if(context.ordered_ranges>policy.maximum_ordered_ranges)
    {result.reason=Reason::fragmented;return result;}
    if(result.qualified_indexed_elements<policy.minimum_qualified_indices)
    {result.reason=Reason::small_work;return result;}
    if(result.qualified_indexed_elements<result.indexed_elements-result.indexed_elements/4)
    {result.reason=Reason::unknown_bounds;return result;}
    if(!supported()||!context.pipeline_supports_indirect)
    {result.reason=Reason::unsupported;return result;}
    result.route=Route::gpu_visibility;result.reason=Reason::heavy_animated;return result;
}
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
    const auto expired_animated=[](const Bank& bank)
    {return bank.animated && std::any_of(bank.animated->sources.begin(),bank.animated->sources.end(),
        [](const auto& source){return !source->currentGeneration();});};
    for(auto it=mImpl->cache.begin();it!=mImpl->cache.end();)
        if(((*it)->membership_pointer&&(*it)->membership.expired())||expired_animated(**it)){it=mImpl->cache.erase(it);++mImpl->statistics.evictions;}
        else ++it;
    std::erase_if(mImpl->indices,[](const auto& value){return value.second.expired();});
}
void ALMetalWorldSourceCommands::failNextPublicationForTesting()noexcept{mImpl->force_failure=true;}
void ALMetalWorldSourceCommands::failExecutionAfterForTesting(std::size_t successful_ranges)noexcept
{mImpl->execution_failure_after=successful_ranges;}
void ALMetalWorldSourceCommands::failVisibilityAfterBeginForTesting()noexcept
{mImpl->fail_visibility_after_begin=true;}
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
        RP::Description backing;backing.kind=RP::Kind::indirect_commands;
        backing.declared_bytes=bank->native.allocatedSize;
        if(!state.wrapper->registerNativeBacking((__bridge void*)bank->native,backing,bank->plan_view,error))
            return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Native command backing registration failed");
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
    impl->command=(__bridge id<MTLCommandBuffer>)const_cast<void*>(command.nativeIdentity());impl->device=state.device;
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
{return prepareCohortForCommandImpl(command,std::move(frame),nullptr,nullptr,error,failure,nullptr);}

std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareVisibleCohortForCommand(
    ALMetalCommandBuffer& command,std::shared_ptr<const ALMetalWorldSourceFrameView> frame,
    const ALMetalDevice& device,const ALMetalShaderLibrary& library,std::string* error,ALMetalWorldSourceCommandFailure* failure)
{return prepareCohortForCommandImpl(command,std::move(frame),&device,&library,error,failure,nullptr);}

ALMetalWorldSourceCommands::Preparation ALMetalWorldSourceCommands::prepareVisibleCohortChecked(
    ALMetalCommandBuffer& command,std::shared_ptr<const ALMetalWorldSourceFrameView> frame,
    const ALMetalDevice& device,const ALMetalShaderLibrary& library,std::string* error,
    ALMetalWorldSourceCommandFailure* failure)
{
    Preparation result;
    result.batch=prepareCohortForCommandImpl(command,std::move(frame),&device,&library,error,failure,&result.native_prefix);
    return result;
}

std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareCohortForCommandImpl(
    ALMetalCommandBuffer& command,std::shared_ptr<const ALMetalWorldSourceFrameView> frame,
    const ALMetalDevice* visibility_device,const ALMetalShaderLibrary* visibility_library,
    std::string* error,ALMetalWorldSourceCommandFailure* failure,bool* native_prefix)
try
{
    if(native_prefix)*native_prefix=false;
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
    id<MTLLibrary> native_visibility_library=nil;
    if(visibility_library)
    {
        if(!direct)return reject(ALMetalWorldSourceCommandFailure::unsupported,"Static GPU visibility requires direct command bindings");
        if(!visibility_device||visibility_device->nativeHandle()!=(__bridge void*)state.device)
            return reject(ALMetalWorldSourceCommandFailure::validation,"Static GPU visibility device owner is invalid");
        native_visibility_library=(__bridge id<MTLLibrary>)visibility_library->nativeHandle();
        if(!native_visibility_library||native_visibility_library.device!=state.device)
            return reject(ALMetalWorldSourceCommandFailure::validation,"Static GPU visibility library belongs to another device");
        if(state.static_visibility_library!=native_visibility_library)
        {
            // Missing old-library entry declines optional preparation before
            // any command encoding; ordinary full-count banks remain usable.
            id<MTLFunction> kernel=[native_visibility_library newFunctionWithName:@"almetal_world_static_visibility_commands"];
            if(!kernel||kernel.functionType!=MTLFunctionTypeKernel)
                return reject(ALMetalWorldSourceCommandFailure::unsupported,"Static GPU visibility entry is unavailable");
            auto root=[kernel newArgumentEncoderWithBufferIndex:2];
            ALMetalComputePipelineDescriptor descriptor;descriptor.library=visibility_library;
            descriptor.function="almetal_world_static_visibility_commands";descriptor.label="Alchemy static GPU command visibility";
            auto pipeline=ALMetalComputePipeline::create(*visibility_device,descriptor,error);
            if(!root||!root.encodedLength||!pipeline)
                return reject(ALMetalWorldSourceCommandFailure::unsupported,"Static GPU visibility layout is unavailable");
            state.static_visibility_root_encoder=root;state.static_visibility_pipeline=std::move(pipeline);
            state.static_visibility_library=native_visibility_library;
        }
    }

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
           bank->static_visibility_library==native_visibility_library&&
           (!direct||bank->direct_normal_hdr==frame->directNormalHDR()))
        {selected=bank;state.cache.splice(state.cache.begin(),state.cache,it);++state.statistics.hits;if(direct)++state.statistics.direct_hits;break;}
        ++it;
    }
    if(!selected)
    {
        if(std::exchange(state.force_failure,false))return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Forced native source command publication failure");
        auto bank=std::make_shared<Bank>();bank->membership=membership;bank->membership_pointer=membership.get();
        bank->direct_slot=slot;bank->direct_normal_hdr=frame->directNormalHDR();
        bank->static_visibility_library=native_visibility_library;
        bank->binding_limit=direct?frame->directBindingAssetLimit():0;
        const auto calls=membership->calls();bank->count=calls.size();
        auto descriptor=[MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=!direct;
        descriptor.maxVertexBufferBindCount=direct?12:0;descriptor.maxFragmentBufferBindCount=direct?2:0;
        bank->native=[state.device newIndirectCommandBufferWithDescriptor:descriptor maxCommandCount:bank->count options:MTLResourceStorageModeShared];
        if(!bank->native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Could not allocate a native cohort command bank");
        RP::Description backing;backing.kind=RP::Kind::indirect_commands;
        backing.declared_bytes=bank->native.allocatedSize;
        if(!state.wrapper->registerNativeBacking((__bridge void*)bank->native,backing,bank->plan_view,error))
            return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Native command backing registration failed");
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
            work.allocation_bytes=bank->bytes;
            if(visibility_library)
            {
                work.encoded_row_bytes=bank->count*sizeof(StaticVisibilityRow)+state.static_visibility_root_encoder.encodedLength;
                work.allocation_bytes+=work.encoded_row_bytes;
            }
            ++state.statistics.admission_requests;
            publication_credit=(*admission)(work);
            if(epoch!=state.epoch||command.isCommitted()||command.nativeDevice()!=(__bridge void*)state.device)
                return reject(ALMetalWorldSourceCommandFailure::admission,"Native cohort command publication changed during admission");
            if(!publication_credit){++state.statistics.admission_denials;return reject(ALMetalWorldSourceCommandFailure::admission,"Native source command publication was deferred");}
            charged_arguments=argument_bytes;charged_bindings=binding_bytes;
        }
        if(visibility_library)
        {
            const auto make=[&](std::size_t bytes,const char* label)
            {
                ALMetalBufferDescriptor descriptor;descriptor.length=bytes;descriptor.label=label;
                return ALMetalBuffer::create(*visibility_device,descriptor,error);
            };
            bank->static_visibility_rows=make(bank->count*sizeof(StaticVisibilityRow),"Alchemy immutable static visibility rows");
            bank->static_visibility_root=make(state.static_visibility_root_encoder.encodedLength,"Alchemy static visibility command root");
            if(!bank->static_visibility_rows||!bank->static_visibility_root)
                return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Static GPU visibility metadata allocation failed");
            for(const auto& buffer:{bank->static_visibility_rows,bank->static_visibility_root})
            {
                auto lease=std::make_shared<IndexLease>();lease->native=(__bridge id<MTLBuffer>)buffer->nativeHandle();
                lease->index_role=false;lease->bytes=lease->native.allocatedSize;lease->accounting=state.accounting;
                state.accounting->binding_bytes.fetch_add(lease->bytes,std::memory_order_relaxed);
                bank->indices.push_back(std::move(lease));
            }
            auto* rows=static_cast<StaticVisibilityRow*>(bank->static_visibility_rows->mappedBytes());
            if(!rows||slot->globalsOffset()>std::numeric_limits<std::uint32_t>::max())
                return reject(ALMetalWorldSourceCommandFailure::validation,"Static GPU visibility mapped range is invalid");
            for(std::size_t index=0;index<calls.size();++index)
            {
                const auto& call=calls[index];const auto& binding=bindings[index];
                const auto region=slot->regionOffset(binding.region);
                id<MTLBuffer> indices=(__bridge id<MTLBuffer>)call.native_indices;
                if(region>std::numeric_limits<std::uint32_t>::max()||!indices.gpuAddress||
                   call.index_offset>std::numeric_limits<std::uint64_t>::max()-indices.gpuAddress)
                    return reject(ALMetalWorldSourceCommandFailure::validation,"Static GPU visibility native address is invalid");
                StaticVisibilityRow row;row.minimum=binding.bounds_minimum;row.maximum=binding.bounds_maximum;
                (call.index_type==ALMetalIndexType::uint16?row.index16:row.index32)=indices.gpuAddress+call.index_offset;
                row.call={call.index_count,static_cast<std::uint32_t>(region),call.row_id,binding.bounds_qualified?1u:0u};
                rows[index]=row;
                bank->static_visibility_qualified_count+=binding.bounds_qualified?1u:0u;
            }
            // Construct the exact index residency union once. Warm dispatch
            // declares this immutable span with one Metal call, never N leases.
            for(const auto& lease:bank->indices)if(lease->index_role)
                bank->static_visibility_indices.push_back((__bridge void*)lease->native);
            auto root=state.static_visibility_root_encoder;
            [root setArgumentBuffer:(__bridge id<MTLBuffer>)bank->static_visibility_root->nativeHandle() offset:0];
            struct Detach{id<MTLArgumentEncoder> encoder;~Detach(){[encoder setArgumentBuffer:nil offset:0];}}detach{root};
            [root setIndirectCommandBuffer:bank->native atIndex:0];
            const std::uint32_t config[]{static_cast<std::uint32_t>(bank->count),static_cast<std::uint32_t>(slot->globalsOffset()),0,0};
            auto* configuration=[root constantDataAtIndex:1];
            if(!configuration)return reject(ALMetalWorldSourceCommandFailure::validation,"Static GPU visibility root layout is invalid");
            std::memcpy(configuration,config,sizeof(config));
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
    impl->command=(__bridge id<MTLCommandBuffer>)const_cast<void*>(command.nativeIdentity());impl->device=state.device;
    auto result=std::shared_ptr<const Batch>(new Batch(std::move(impl)));
    if(!retainPublication(command,result,std::move(publication_credit),error))return reject(ALMetalWorldSourceCommandFailure::retention,"Native source command lease retention failed");
    if(visibility_library)
    {
        const auto& bank=*result->mImpl->bank;
        if(native_prefix)*native_prefix=true;
        auto compute=ALMetalComputeEncoder::begin(command,"Alchemy static GPU command visibility",error);
        if(std::exchange(state.fail_visibility_after_begin,false))
            return reject(ALMetalWorldSourceCommandFailure::execution,"Forced static visibility failure after begin");
        if(!compute||!compute->setPipeline(*state.static_visibility_pipeline,error)||
           !compute->setBuffer(0,*bank.static_visibility_rows,0,error)||
           !compute->setBuffer(2,*bank.static_visibility_root,0,error))
            return reject(ALMetalWorldSourceCommandFailure::execution,"Static GPU visibility compute setup failed");
        if(!compute->declareResourceUse(bank.plan_view,RP::Access::write,error)||
           !compute->declareResourceUse(slot->resourceView(),RP::Access::read,error))
            return reject(ALMetalWorldSourceCommandFailure::execution,"Static visibility resource declaration failed");
        if(membership->indexResourcesComplete())
        {
            for(const auto& resource:membership->indexResourceViews())
                if(!compute->declareResourceUse(resource,RP::Access::read,error))
                    return reject(ALMetalWorldSourceCommandFailure::execution,"Static visibility index declaration failed");
        }
        else (void)compute->nativeHandleForOpaqueResources(); // Opaque legacy certificate.
        auto native=(__bridge id<MTLComputeCommandEncoder>)compute->nativeHandleForDeclaredResources();
        if(!native)return reject(ALMetalWorldSourceCommandFailure::execution,"Static GPU visibility compute encoder is unavailable");
        [native setBuffer:(__bridge id<MTLBuffer>)slot->nativeBuffer() offset:0 atIndex:1];
        // The slot is fixed-address immutable while this exact FrameView owns
        // its reader. Native index leases make the cold GPU pointers safe;
        // no additional idle direct-slot reader or source graph is retained.
        if(!bank.static_visibility_indices.empty())
        {
            const auto* indices=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(bank.static_visibility_indices.data());
            [native useResources:indices count:bank.static_visibility_indices.size() usage:MTLResourceUsageRead];
        }
        [native useResource:bank.native usage:MTLResourceUsageWrite];
        if(!compute->dispatch(static_cast<std::uint32_t>(bank.count),1,1,32,1,1,error)||!compute->end(error))
            return reject(ALMetalWorldSourceCommandFailure::execution,"Static GPU visibility dispatch failed");
        ++state.statistics.static_visibility_dispatches;state.statistics.static_visibility_rows+=bank.count;
        state.statistics.static_visibility_qualified_rows+=bank.static_visibility_qualified_count;
    }
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

ALMetalWorldSourceCommands::DeclarationScope ALMetalWorldSourceCommands::makeDeclarationScope(
    ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,std::string* error)
try
{
    if(!encoder.nativeHandleForDeclaredResources() || command.isCommitted() ||
       encoder.nativeDeviceForIndirect()!=command.nativeDevice() ||
       encoder.nativeCommandBufferIdentity()!=const_cast<void*>(command.nativeIdentity()))
    {if(error)*error="Native declaration scope requires its active command encoder";return {};}
    return DeclarationScope(encoder.declarationIdentity(true));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

bool ALMetalWorldSourceCommands::executeRange(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const Batch& batch,std::size_t first,std::size_t count,std::string* error)
{return executeRangeImpl(encoder,command,batch,first,count,nullptr,error);}

bool ALMetalWorldSourceCommands::executeRange(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const Batch& batch,std::size_t first,std::size_t count,const DeclarationScope& scope,std::string* error)
{return executeRangeImpl(encoder,command,batch,first,count,&scope,error);}

bool ALMetalWorldSourceCommands::executeRangeImpl(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    const Batch& batch,std::size_t first,std::size_t count,const DeclarationScope* scope,std::string* error)
{
    const auto& prepared=*batch.mImpl;
    if(!count||first>prepared.bank->count||count>prepared.bank->count-first||
        !encoder.canExecuteIndirect()||encoder.nativeDeviceForIndirect()!=(__bridge void*)prepared.device||
        encoder.nativeCommandBufferIdentity()!=(__bridge void*)prepared.command||const_cast<void*>(command.nativeIdentity())!=(__bridge void*)prepared.command||command.isCommitted())
    {mImpl->fail(ALMetalWorldSourceCommandFailure::execution,nullptr,error,"Native source bank requires its prepared command and compatible pipeline");return false;}
    std::shared_ptr<const void> identity;
    if(scope)
    {
        identity=encoder.declarationIdentity(false);
        if(!identity || !scope->mIdentity || scope->mIdentity.get()!=identity.get() ||
           scope->mIdentity.owner_before(identity) || identity.owner_before(scope->mIdentity))
        {mImpl->fail(ALMetalWorldSourceCommandFailure::execution,nullptr,error,"Native declaration scope is stale or belongs to another encoder");return false;}
    }
    if(mImpl->execution_failure_after&&*mImpl->execution_failure_after==0)
    {
        mImpl->execution_failure_after.reset();
        mImpl->fail(ALMetalWorldSourceCommandFailure::execution,nullptr,error,"Forced native source range execution failure");
        return false;
    }
    if(prepared.frame && !prepared.frame->bind(encoder,command,error))return false;
    if(prepared.bank->animated)
    {
        const auto& previous=prepared.declared_encoder_identity;
        const bool declared=scope && !previous.owner_before(identity) && !identity.owner_before(previous);
        if(!declared)
        {
            ++mImpl->statistics.declaration_calls;
            for(const auto& source:prepared.bank->animated->sources)
                if(!source->declareResources(encoder,command,error))return false;
            if(!encoder.declareResourceUse(prepared.bank->animated->views->resourceView(),RP::Access::read,error))return false;
            for(const auto& slice:prepared.animated_palettes)
                if(!encoder.declareResourceUse(slice.buffer->resourceView(),RP::Access::read,error))return false;
            auto declarations=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForDeclaredResources();
            if(!declarations)return false;
            for(const auto& lease:prepared.bank->indices)
                [declarations useResource:lease->native usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
            for(const auto& slice:prepared.animated_palettes)
                [declarations useResource:(__bridge id<MTLBuffer>)slice.buffer->nativeHandle() usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
            [declarations useResource:prepared.bank->native usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
            // The actual Batch owns this exact native tuple and late palette
            // slices. A new Batch over the same Bank starts with no proof.
            // Weak identity assignment cannot allocate and follows ALL calls.
            if(scope)batch.mImpl->declared_encoder_identity=identity;
        }
    }
    if(!encoder.declareResourceUse(prepared.bank->plan_view,RP::Access::read,error))return false;
    encoder.invalidateIndirectBufferBindings();
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForDeclaredResources();
    if(!native)return false;
    if(!prepared.bank->animated)
        [native useResource:prepared.bank->native usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
    // matchesSourceCommand proved each index belongs to this FrameView;
    // frame.bind declared its exact geometry/index union once per encoder.
    [native executeCommandsInBuffer:prepared.bank->native withRange:NSMakeRange(first,count)];
    encoder.recordIndirectSubmission(static_cast<std::uint32_t>(count));
    if(mImpl->execution_failure_after)--*mImpl->execution_failure_after;
    // Direct banks own their buffer binds. Subsequent ordinary bridge calls
    // must restore their slots even if its prior cached pointer was equal.
    if(prepared.bank->direct_slot||prepared.bank->animated)encoder.invalidateIndirectBufferBindings();
    ++mImpl->statistics.executions;mImpl->statistics.logical_draws+=count;return true;
}

std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareAnimatedForCommand(
    ALMetalCommandBuffer& command,const ALMetalDevice& device,const ALMetalShaderLibrary& library,
    std::span<const ALMetalWorldAnimatedGpuView> views,const ALMetalWorldSourceGlobals& globals,
    ALMetalWorldFrameData& frame_data,std::string* error,bool gpu_visibility)
try
{
    if(views.empty()||views.size()>alMetalWorldSourceMaximumRows)
    {
        ++mImpl->statistics.requests;
        mImpl->fail(ALMetalWorldSourceCommandFailure::validation,nullptr,error,"Animated native command input is invalid");return {};
    }
    // A borrowed caller can still mutate its vector during admission. Capture
    // the complete value before entering the owned preparation boundary.
    return prepareAnimatedForCommand(command,device,library,
        std::vector<ALMetalWorldAnimatedGpuView>(views.begin(),views.end()),globals,frame_data,error,gpu_visibility);
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareAnimatedForCommand(
    ALMetalCommandBuffer& command,const ALMetalDevice& device,const ALMetalShaderLibrary& library,
    std::vector<ALMetalWorldAnimatedGpuView>&& views,const ALMetalWorldSourceGlobals& borrowed_globals,
    ALMetalWorldFrameData& frame_data,std::string* error,bool gpu_visibility)
{
    return prepareAnimatedForCommandImpl(command,device,library,std::move(views),borrowed_globals,
        frame_data,error,gpu_visibility,nullptr);
}

ALMetalWorldSourceCommands::Preparation ALMetalWorldSourceCommands::prepareAnimatedChecked(
    ALMetalCommandBuffer& command,const ALMetalDevice& device,const ALMetalShaderLibrary& library,
    std::vector<ALMetalWorldAnimatedGpuView>&& views,const ALMetalWorldSourceGlobals& globals,
    ALMetalWorldFrameData& frame_data,std::string* error,bool gpu_visibility)
{
    Preparation result;
    result.batch=prepareAnimatedForCommandImpl(command,device,library,std::move(views),globals,
        frame_data,error,gpu_visibility,&result.native_prefix);
    return result;
}

std::shared_ptr<const ALMetalWorldSourceCommands::Batch> ALMetalWorldSourceCommands::prepareAnimatedForCommandImpl(
    ALMetalCommandBuffer& command,const ALMetalDevice& device,const ALMetalShaderLibrary& library,
    std::vector<ALMetalWorldAnimatedGpuView>&& views,const ALMetalWorldSourceGlobals& borrowed_globals,
    ALMetalWorldFrameData& frame_data,std::string* error,bool gpu_visibility,bool* native_prefix)
try
{
    if(native_prefix)*native_prefix=false;
    // Consume ownership immediately. The renderer has already produced this
    // vector privately; no second matrix/lease copy or allocation is needed.
    auto inputs=std::move(views);
    const auto globals=borrowed_globals; // Admission cannot replace bank/pose flags or mirror data.
    auto& state=*mImpl;++state.statistics.requests;
    const auto reject=[&](auto reason,const char* message)->std::shared_ptr<const Batch>
        {state.fail(reason,nullptr,error,message);return {};};
    if(state.preparing||!state.supported||inputs.empty()||inputs.size()>alMetalWorldSourceMaximumRows||
       device.nativeHandle()!=(__bridge void*)state.device||command.nativeDevice()!=device.nativeHandle()||command.isCommitted())
        return reject(ALMetalWorldSourceCommandFailure::validation,"Animated native command input is invalid");
    struct Guard{bool& active;~Guard(){active=false;}}guard{state.preparing};state.preparing=true;
    id<MTLLibrary> native_library=(__bridge id<MTLLibrary>)library.nativeHandle();
    if(state.animated_library!=native_library)
    {
        id<MTLFunction> vertex=[native_library newFunctionWithName:@"almetal_world_material_animated_vertex"];
        if(!vertex||vertex.functionType!=MTLFunctionTypeVertex)
            return reject(ALMetalWorldSourceCommandFailure::unsupported,"Animated native vertex entry is unavailable");
        auto pose=[vertex newArgumentEncoderWithBufferIndex:4];
        if(!pose||!pose.encodedLength||!pose.alignment)
            return reject(ALMetalWorldSourceCommandFailure::unsupported,"Animated native pose layout is unavailable");
        state.pose_encoder=pose;state.root_encoder=nil;state.animated_pipeline.reset();state.animated_library=native_library;
    }
    // Full-count banks use the vertex PoseView layout directly. They require
    // no compute function, root argument encoder or compute pipeline owner.
    if(gpu_visibility&&!state.animated_pipeline)
    {
        id<MTLFunction> kernel=[native_library newFunctionWithName:@"almetal_world_animated_visibility"];
        if(!kernel||kernel.functionType!=MTLFunctionTypeKernel)
            return reject(ALMetalWorldSourceCommandFailure::unsupported,"Animated native visibility entry is unavailable");
        auto root=[kernel newArgumentEncoderWithBufferIndex:0];
        ALMetalComputePipelineDescriptor descriptor;descriptor.library=&library;
        descriptor.function="almetal_world_animated_visibility";descriptor.label="Alchemy current-pose animated visibility";
        auto pipeline=ALMetalComputePipeline::create(device,descriptor,error);
        if(!root||!root.encodedLength||!pipeline)
            return reject(ALMetalWorldSourceCommandFailure::unsupported,"Animated native visibility layout is unavailable");
        state.root_encoder=root;state.animated_pipeline=std::move(pipeline);
    }
    // Late CPU inputs are privately owned before admission can invoke
    // application callbacks; CPU records retire after synchronous encoding.
    for(const auto& view:inputs)
    {
        if(!view.source||!view.pose||!view.source->matchesNativeLease(view.native)||
           !ALMetalWorldSourceCohort::validView(view.view))
            return reject(ALMetalWorldSourceCommandFailure::validation,"Animated source, pose or native/view proof is invalid");
    }
    trimUnusedBanks();
    std::shared_ptr<const Bank> selected;std::optional<BankPublication> publication;
    std::shared_ptr<const void> credit;
    for(auto it=state.cache.begin();it!=state.cache.end();++it)
    {
        const auto& bank=*it;const auto& slot=bank->animated;
        if(!slot||!slot->reader.expired()||slot->library!=(__bridge void*)native_library||
           slot->normal_hdr!=(globals.flags[2]!=0)||slot->gpu_visibility!=gpu_visibility||slot->sources.size()!=inputs.size())continue;
        bool same=true;for(std::size_t i=0;i<inputs.size();++i)if(slot->sources[i]!=inputs[i].source){same=false;break;}
        if(same){selected=bank;state.cache.splice(state.cache.begin(),state.cache,it);++state.statistics.hits;break;}
    }
    if(!selected)
    {
        if(std::exchange(state.force_failure,false))return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Forced animated command publication failure");
        auto bank=std::make_shared<Bank>();bank->count=inputs.size();bank->animated=std::make_shared<AnimatedSlot>();
        auto& slot=*bank->animated;slot.library=(__bridge void*)native_library;slot.normal_hdr=globals.flags[2]!=0;
        // A GPU-mutated bank can contain zero counts. Full-count mode must
        // never reuse that bank, even after all its readers have completed.
        slot.gpu_visibility=gpu_visibility;
        const auto alignment=state.pose_encoder.alignment;
        if(state.pose_encoder.encodedLength>std::numeric_limits<std::size_t>::max()-(alignment-1))return {};
        slot.stride=(state.pose_encoder.encodedLength+alignment-1)/alignment*alignment;
        if(slot.stride>std::numeric_limits<std::size_t>::max()/inputs.size())return {};
        const auto make=[&](std::size_t length,const char* label)->std::shared_ptr<ALMetalBuffer>
        {ALMetalBufferDescriptor d;d.length=length;d.label=label;return ALMetalBuffer::create(device,d,error);};
        // Reserve optional cold work before the first allocation or native
        // command encoding. Warm pose/view publication is required work.
        const auto epoch=state.epoch;ALMetalWorldSourcePublicationWork work;
        work.encoded_bank_range_bytes=inputs.size()*248;
        work.encoded_row_bytes=slot.stride*inputs.size()+(gpu_visibility?state.root_encoder.encodedLength:0);
        if(state.admission)
        {
            ++state.statistics.admission_requests;auto policy=state.admission;credit=(*policy)(work);
            if(!credit){++state.statistics.admission_denials;return reject(ALMetalWorldSourceCommandFailure::admission,"Animated command publication deferred");}
            if(epoch!=state.epoch||command.isCommitted())return reject(ALMetalWorldSourceCommandFailure::admission,"Animated command context changed during admission");
        }
        slot.views=make(slot.stride*inputs.size(),"Alchemy animated pose/view slots");
        if(gpu_visibility)
        {
            slot.root=make(state.root_encoder.encodedLength,"Alchemy animated compute pointer table");
            slot.visible=make(inputs.size()*sizeof(std::uint32_t),"Alchemy animated GPU visibility results");
        }
        if(!slot.views||(gpu_visibility&&(!slot.root||!slot.visible)))return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Animated slot allocation failed");
        auto descriptor=[MTLIndirectCommandBufferDescriptor new];descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;
        descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=NO;
        descriptor.maxVertexBufferBindCount=13;descriptor.maxFragmentBufferBindCount=2;
        bank->native=[state.device newIndirectCommandBufferWithDescriptor:descriptor maxCommandCount:bank->count options:MTLResourceStorageModeShared];
        if(!bank->native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Animated command bank allocation failed");
        RP::Description backing;backing.kind=RP::Kind::indirect_commands;
        backing.declared_bytes=bank->native.allocatedSize;
        if(!state.wrapper->registerNativeBacking((__bridge void*)bank->native,backing,bank->plan_view,error))
            return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Native command backing registration failed");
        bank->bytes=bank->native.allocatedSize;bank->accounting=state.accounting;
        state.accounting->icb_bytes.fetch_add(bank->bytes,std::memory_order_relaxed);state.accounting->banks.fetch_add(1,std::memory_order_relaxed);
        bank->binding_limit=std::clamp<std::uint64_t>(device.info().recommended_working_set_size/16,256ULL*1024*1024,4ULL*1024*1024*1024);
        std::unordered_map<ResourceKey,std::shared_ptr<IndexLease>,ResourceKeyHash> unique;
        const auto retain=[&](void* handle,bool index=false)
        {
            ResourceKey key{handle,index};if(!handle)return false;if(unique.contains(key))return true;
            std::shared_ptr<IndexLease> lease;if(auto found=state.indices.find(key);found!=state.indices.end())lease=found->second.lock();
            if(!lease)
            {
                lease=std::make_shared<IndexLease>();lease->native=(__bridge id<MTLResource>)handle;lease->index_role=index;
                if(lease->native.device!=state.device||!lease->native.allocatedSize)return false;
                lease->bytes=lease->native.allocatedSize;lease->accounting=state.accounting;
                (index?state.accounting->index_bytes:state.accounting->binding_bytes).fetch_add(lease->bytes,std::memory_order_relaxed);
            }
            unique.emplace(key,lease);bank->indices.push_back(std::move(lease));return true;
        };
        if(!retain(slot.views->nativeHandle()))return {};
        if(gpu_visibility&&(!retain(slot.root->nativeHandle())||!retain(slot.visible->nativeHandle())))return {};
        for(std::size_t index=0;index<inputs.size();++index)
        {
            auto source=inputs[index].source;const auto& b=source->binding();
            if(!retain(b.indices,true)||!retain(b.policy->nativeHandle())||!retain(b.bones->nativeHandle()))return {};
            for(auto geometry:b.geometry)if(!retain(geometry))return {};
            const auto numeric=b.numeric[b.pbr?(slot.normal_hdr?2:1):0];
            const auto table=source->table()->argumentLease();
            if(!numeric||!table||!retain(numeric)||!retain(table->nativeHandle()))return {};
            for(const auto& asset:source->assets())if(!retain(asset.allocation))return {};
            slot.sources.push_back(std::move(source));bank->direct_tables.push_back(slot.sources.back()->table());
            id<MTLIndirectRenderCommand> call=[bank->native indirectRenderCommandAtIndex:index];if(!call)return {};
            constexpr unsigned bindings[]{0,1,2,3,8,9,10,12};
            for(unsigned at=0;at<8;++at)[call setVertexBuffer:(__bridge id<MTLBuffer>)b.geometry[at] offset:b.offsets[at] atIndex:bindings[at]];
            [call setVertexBuffer:(__bridge id<MTLBuffer>)slot.views->nativeHandle() offset:index*slot.stride atIndex:4];
            [call setVertexBuffer:(__bridge id<MTLBuffer>)b.policy->nativeHandle() offset:0 atIndex:6];
            if(b.pbr)[call setVertexBuffer:(__bridge id<MTLBuffer>)numeric offset:0 atIndex:11];
            [call setFragmentBuffer:(__bridge id<MTLBuffer>)numeric offset:0 atIndex:0];
            [call setFragmentBuffer:(__bridge id<MTLBuffer>)table->nativeHandle() offset:0 atIndex:1];
            [call drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:b.index_count
                indexType:b.index_type==ALMetalIndexType::uint16?MTLIndexTypeUInt16:MTLIndexTypeUInt32
                indexBuffer:(__bridge id<MTLBuffer>)b.indices indexBufferOffset:b.index_offset instanceCount:1 baseVertex:0 baseInstance:0];
        }
        if(gpu_visibility)
        {
            // Pointer table contains the same reflected PoseView ABI used by VS.
            [state.root_encoder setArgumentBuffer:(__bridge id<MTLBuffer>)slot.root->nativeHandle() offset:0];
            struct Detach{id<MTLArgumentEncoder> encoder;~Detach(){[encoder setArgumentBuffer:nil offset:0];}}detach{state.root_encoder};
            for(std::size_t index=0;index<inputs.size();++index)
                [state.root_encoder setBuffer:(__bridge id<MTLBuffer>)slot.views->nativeHandle() offset:index*slot.stride atIndex:index];
            [state.root_encoder setIndirectCommandBuffer:bank->native atIndex:4096];
            const std::uint32_t config[]{static_cast<std::uint32_t>(inputs.size()),0,0,0};
            std::memcpy([state.root_encoder constantDataAtIndex:4097],config,sizeof(config));
        }
        selected=std::move(bank);publication.emplace();
        if(!state.planPublication(selected,*publication))return reject(ALMetalWorldSourceCommandFailure::capacity,"Animated live command/asset budget is full");
    }
    auto impl=std::make_unique<Batch::Impl>();impl->bank=selected;impl->command=(__bridge id<MTLCommandBuffer>)const_cast<void*>(command.nativeIdentity());impl->device=state.device;
    impl->animated_reader=std::make_shared<const std::uint8_t>(0);
    impl->animated_native.reserve(inputs.size());impl->animated_palettes.reserve(inputs.size());
    for(const auto& input:inputs)
    {
        ALMetalWorldDataSlice slice;
        if(!frame_data.publishPaletteRecord(command,input.pose.get(),slice,error,true))return {};
        impl->animated_palettes.push_back(slice);impl->animated_native.push_back(input.native);
    }
    auto result=std::shared_ptr<const Batch>(new Batch(std::move(impl)));
    if(!retainPublication(command,result,std::move(credit),error))return reject(ALMetalWorldSourceCommandFailure::retention,"Animated native command retention failed");
    auto& slot=*selected->animated;slot.reader=result->mImpl->animated_reader;
    struct Detach{id<MTLArgumentEncoder> encoder;~Detach(){[encoder setArgumentBuffer:nil offset:0];}}detach{state.pose_encoder};
    for(std::size_t index=0;index<inputs.size();++index)
    {
        const auto& input=inputs[index];const auto& b=input.source->binding();const auto& palette=result->mImpl->animated_palettes[index];
        [state.pose_encoder setArgumentBuffer:(__bridge id<MTLBuffer>)slot.views->nativeHandle() offset:index*slot.stride];
        [state.pose_encoder setBuffer:(__bridge id<MTLBuffer>)palette.buffer->nativeHandle() offset:palette.offset atIndex:0];
        [state.pose_encoder setBuffer:(__bridge id<MTLBuffer>)b.bones->nativeHandle() offset:b.bone_offset atIndex:1];
        [state.pose_encoder setBuffer:b.index_type==ALMetalIndexType::uint16?(__bridge id<MTLBuffer>)b.indices:nil offset:b.index_type==ALMetalIndexType::uint16?b.index_offset:0 atIndex:2];
        [state.pose_encoder setBuffer:b.index_type==ALMetalIndexType::uint32?(__bridge id<MTLBuffer>)b.indices:nil offset:b.index_type==ALMetalIndexType::uint32?b.index_offset:0 atIndex:3];
        std::memcpy([state.pose_encoder constantDataAtIndex:4],input.view.modelview_projection,64);
        std::memcpy([state.pose_encoder constantDataAtIndex:5],input.view.modelview,64);
        std::memcpy([state.pose_encoder constantDataAtIndex:6],input.view.rigged_normal_matrix,48);
        std::memcpy([state.pose_encoder constantDataAtIndex:7],globals.mirror_clip_plane,16);
        const std::uint32_t flags[]{globals.flags[0],globals.flags[1],input.pose->jointCount(),b.bone_count};
        const std::uint32_t call[]{b.index_count,b.index_type==ALMetalIndexType::uint32,b.qualified,0};
        std::memcpy([state.pose_encoder constantDataAtIndex:8],flags,16);std::memcpy([state.pose_encoder constantDataAtIndex:9],call,16);
    }
    if(gpu_visibility)
    {
        if(native_prefix)*native_prefix=true;
        auto compute=ALMetalComputeEncoder::begin(command,"Alchemy retained animated current-pose visibility",error);
        if(std::exchange(state.fail_visibility_after_begin,false))
            return reject(ALMetalWorldSourceCommandFailure::execution,"Forced animated visibility failure after begin");
        if(!compute||!compute->setPipeline(*state.animated_pipeline,error)||!compute->setBuffer(0,*slot.root,0,error)||
           !compute->setBuffer(1,*slot.visible,0,error))return {};
        if(!compute->declareResourceUse(selected->plan_view,RP::Access::write,error)||
           !compute->declareResourceUse(slot.views->resourceView(),RP::Access::read,error))return {};
        for(const auto& source:slot.sources)if(!source->declareResourceUses(*compute,error))return {};
        for(const auto& palette:result->mImpl->animated_palettes)
            if(!compute->declareResourceUse(palette.buffer->resourceView(),RP::Access::read,error))return {};
        auto native=(__bridge id<MTLComputeCommandEncoder>)compute->nativeHandleForDeclaredResources();if(!native)return {};
        for(const auto& lease:selected->indices)[native useResource:lease->native usage:MTLResourceUsageRead];
        for(const auto& palette:result->mImpl->animated_palettes)[native useResource:(__bridge id<MTLBuffer>)palette.buffer->nativeHandle() usage:MTLResourceUsageRead];
        [native useResource:selected->native usage:MTLResourceUsageWrite];
        if(!compute->dispatch(static_cast<std::uint32_t>(inputs.size()),1,1,32,1,1,error)||!compute->end(error))return {};
    }
    if(publication){state.commitPublication(*publication);state.binding_limit=selected->binding_limit;
        state.statistics.charged_logical_binding_bytes+=inputs.size()*248;}
    state.statistics.last_failure=ALMetalWorldSourceCommandFailure::none;return result;
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}
catch(...){if(error)error->clear();return {};}
