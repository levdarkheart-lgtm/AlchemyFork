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
    std::atomic<std::uint64_t> icb_bytes{0},index_bytes{0};
    std::atomic<std::size_t> banks{0};
};
struct IndexLease
{
    id<MTLBuffer> native=nil;
    std::shared_ptr<Accounting> accounting;
    std::uint64_t bytes=0;
    ~IndexLease()
    {
        native=nil;
        if(accounting)accounting->index_bytes.fetch_sub(bytes,std::memory_order_relaxed);
    }
};
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
    std::vector<std::shared_ptr<IndexLease>> indices;
    std::shared_ptr<Accounting> accounting;
    std::uint64_t bytes=0,hash=0;
    ~Bank()
    {
        if(accounting)
        {
            native=nil;indices.clear();
            accounting->icb_bytes.fetch_sub(bytes,std::memory_order_relaxed);
            accounting->banks.fetch_sub(1,std::memory_order_relaxed);
        }
    }
};
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
    std::uint64_t epoch=1;
    ALMetalWorldSourceCommandLimits limits;
    ALMetalWorldSourceCommandStats statistics;
    std::shared_ptr<ALMetalWorldSourcePages::PublicationAdmission> admission;
    std::shared_ptr<Accounting> accounting=std::make_shared<Accounting>();
    std::list<std::shared_ptr<const Bank>> cache;
    std::unordered_map<void*,std::weak_ptr<IndexLease>> indices;
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
        return icb>limits.maximum_live_native_bytes || index>limits.maximum_live_native_bytes-icb ||
            accounting->banks.load(std::memory_order_relaxed)>limits.maximum_live_banks;
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
std::size_t ALMetalWorldSourceCommands::Batch::size()const noexcept{return mImpl->bank->keys.size();}
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
void ALMetalWorldSourceCommands::failNextPublicationForTesting()noexcept{mImpl->force_failure=true;}
ALMetalWorldSourceCommandStats ALMetalWorldSourceCommands::stats()const noexcept
{
    auto result=mImpl->statistics;result.cached_banks=mImpl->cache.size();
    result.live_banks=mImpl->accounting->banks.load(std::memory_order_relaxed);
    result.live_icb_allocated_bytes=mImpl->accounting->icb_bytes.load(std::memory_order_relaxed);
    result.unique_retained_index_bytes=mImpl->accounting->index_bytes.load(std::memory_order_relaxed);return result;
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
    if(command.isCommitted()||command.nativeDevice()!=(__bridge void*)state.device||!frame||draws.empty()||draws.size()>alMetalWorldSourceMaximumRows)
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
    for(auto it=state.cache.begin();it!=state.cache.end();++it)
        if((*it)->hash==hash&&(*it)->keys==keys)
        {selected=*it;state.cache.splice(state.cache.begin(),state.cache,it);++state.statistics.hits;break;}
    if(!selected)
    {
        if(std::exchange(state.force_failure,false))return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Forced native source command publication failure");
        auto bank=std::make_shared<Bank>();bank->hash=hash;bank->keys=std::move(keys);
        auto descriptor=[MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=YES;
        descriptor.maxVertexBufferBindCount=0;descriptor.maxFragmentBufferBindCount=0;
        bank->native=[state.device newIndirectCommandBufferWithDescriptor:descriptor maxCommandCount:draws.size() options:MTLResourceStorageModeShared];
        if(!bank->native)return reject(ALMetalWorldSourceCommandFailure::native_allocation,"Could not allocate a native source command bank");
        bank->bytes=bank->native.allocatedSize;bank->accounting=state.accounting;
        state.accounting->icb_bytes.fetch_add(bank->bytes,std::memory_order_relaxed);
        state.accounting->banks.fetch_add(1,std::memory_order_relaxed);
        std::erase_if(state.indices,[](const auto& value){return value.second.expired();});
        for(const auto& draw:draws)
        {
            void* identity=draw.indices->nativeHandle();std::shared_ptr<IndexLease> lease;
            if(const auto found=state.indices.find(identity);found!=state.indices.end())lease=found->second.lock();
            if(!lease)
            {
                while(state.indices.size()>=state.limits.maximum_unique_index_buffers && !state.cache.empty())
                {
                    state.cache.pop_back();++state.statistics.evictions;
                    std::erase_if(state.indices,[](const auto& value){return value.second.expired();});
                }
                if(state.indices.size()>=state.limits.maximum_unique_index_buffers)
                    return reject(ALMetalWorldSourceCommandFailure::capacity,"Native source command index metadata is full");
                lease=std::make_shared<IndexLease>();lease->native=(__bridge id<MTLBuffer>)identity;
                // PPD188: defend the native helper's cold boundary as well
                // as the FrameView publisher's unique-resource device proof.
                if(lease->native.device!=state.device)
                    return reject(ALMetalWorldSourceCommandFailure::validation,"Native source command index belongs to another device");
                lease->bytes=lease->native.allocatedSize;lease->accounting=state.accounting;
                state.accounting->index_bytes.fetch_add(lease->bytes,std::memory_order_relaxed);state.indices[identity]=lease;
            }
            if(std::find(bank->indices.begin(),bank->indices.end(),lease)==bank->indices.end())bank->indices.push_back(std::move(lease));
        }
        while(!state.cache.empty()&&(state.overCapacity()||state.cache.size()>=state.limits.maximum_cached_banks))
        {state.cache.pop_back();++state.statistics.evictions;}
        if(state.overCapacity())return reject(ALMetalWorldSourceCommandFailure::capacity,"Native source command live/retired storage is full");
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
            auto credit=(*admission)(work);
            if(epoch!=state.epoch)return reject(ALMetalWorldSourceCommandFailure::admission,"Native source command publication changed during admission");
            if(!credit){++state.statistics.admission_denials;return reject(ALMetalWorldSourceCommandFailure::admission,"Native source command publication was deferred");}
            if(!command.retainUntilCompletion(std::move(credit),error))
                return reject(ALMetalWorldSourceCommandFailure::retention,"Native source command credit retention failed");
            state.statistics.charged_logical_argument_bytes+=work.encoded_bank_range_bytes;
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
        selected=std::move(bank);++state.statistics.builds;
        if(state.limits.maximum_cached_banks)state.cache.push_front(selected);
    }
    auto impl=std::make_unique<Batch::Impl>();impl->bank=std::move(selected);impl->frame=std::move(frame);
    impl->command=(__bridge id<MTLCommandBuffer>)command.nativeHandle();impl->device=state.device;
    auto result=std::shared_ptr<const Batch>(new Batch(std::move(impl)));
    if(!command.retainUntilCompletion(result,error))return reject(ALMetalWorldSourceCommandFailure::retention,"Native source command lease retention failed");
    state.statistics.last_failure=ALMetalWorldSourceCommandFailure::none;return result;
}
catch(const std::bad_alloc&)
{mImpl->fail(ALMetalWorldSourceCommandFailure::host_allocation,failure,nullptr,"");if(error)error->clear();return {};}
catch(...)
{mImpl->fail(ALMetalWorldSourceCommandFailure::admission,failure,error,"Native source command publication callback failed");return {};}
bool ALMetalWorldSourceCommands::execute(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,const Batch& batch,std::string* error)
{
    const auto& prepared=*batch.mImpl;
    if(!encoder.canExecuteIndirect()||encoder.nativeDeviceForIndirect()!=(__bridge void*)prepared.device||
        encoder.nativeCommandBufferForIndirect()!=(__bridge void*)prepared.command||command.nativeHandle()!=(__bridge void*)prepared.command||command.isCommitted())
    {mImpl->fail(ALMetalWorldSourceCommandFailure::execution,nullptr,error,"Native source bank requires its prepared command and compatible pipeline");return false;}
    if(!prepared.frame->bind(encoder,command,error))return false;
    id<MTLRenderCommandEncoder> native=(__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForIndirect();
    if(!native)return false;
    [native useResource:prepared.bank->native usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
    // matchesSourceCommand proved each index belongs to this FrameView;
    // frame.bind declared its exact geometry/index union once per encoder.
    [native executeCommandsInBuffer:prepared.bank->native withRange:NSMakeRange(0,prepared.bank->keys.size())];
    encoder.recordIndirectSubmission(static_cast<std::uint32_t>(prepared.bank->keys.size()));
    // inheritBuffers=YES leaves bindings unchanged; bridge flushes scalar joins.
    ++mImpl->statistics.executions;mImpl->statistics.logical_draws+=prepared.bank->keys.size();return true;
}
