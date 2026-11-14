/** Shared persistent conservative bounds and ordered same-command arguments. */
#include "almetalworldvisibility.h"
#include "almetalworldvisibilitysource.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <unordered_map>

namespace
{
constexpr std::size_t maximum_records=65536, maximum_argument_slots=32;
struct alignas(16) BoundsRecord
{
    float minimum[4]{},maximum[4]{};
    std::uint64_t identity=0,revision=0;
    std::uint32_t config[4]{};
};
static_assert(sizeof(BoundsRecord)==64);
void fail(std::string* error,const char* message){if(error)*error=message;}
void fallback(ALMetalWorldVisibilityStats& stats,ALMetalWorldVisibilityFailure reason) noexcept
{++stats.fallbacks;++stats.fallback_reasons[static_cast<std::size_t>(reason)];stats.last_fallback=reason;}
struct BoundsGeneration
{
    std::unique_ptr<ALMetalBuffer> buffer;
    std::vector<std::shared_ptr<const ALMetalCommandCompletion>> readers;
    std::uint64_t serial=0;
    bool complete() const noexcept
    {return std::all_of(readers.begin(),readers.end(),[](const auto& value){return value->isComplete();});}
};
}
struct ALMetalWorldVisibilityArguments::Impl
{
    std::unique_ptr<ALMetalBuffer> members,arguments;
    std::shared_ptr<BoundsGeneration> bounds;
    std::shared_ptr<const ALMetalCommandCompletion> consumer;
    id<MTLCommandBuffer> native_command=nil;
    std::vector<std::array<std::uint32_t,2>> admitted; // original index count, enabled
    std::uint64_t generation=0;
};
struct ALMetalWorldVisibility::Impl
{
    const ALMetalDevice* device=nullptr;
    id<MTLDevice> native_device=nil;
    std::unique_ptr<ALMetalShaderLibrary> library;
    id<MTLComputePipelineState> pipeline=nil;
    std::vector<BoundsRecord> records;
    std::vector<bool> dirty;
    std::vector<std::uint32_t> free_slots;
    std::unordered_map<std::uint64_t,std::uint32_t> identities;
    bool has_dirty=false;
    std::shared_ptr<BoundsGeneration> current;
    std::vector<std::shared_ptr<ALMetalWorldVisibilityArguments::Impl>> slots;
    ALMetalWorldVisibilityStats statistics;
    std::uint64_t next_generation=0;
};
struct ALMetalWorldVisibilityView::Impl
{
    std::shared_ptr<ALMetalWorldVisibility::Impl> owner;
    const std::vector<ALMetalWorldVisibilityMember> members;
    Impl(std::shared_ptr<ALMetalWorldVisibility::Impl> value,std::vector<ALMetalWorldVisibilityMember> input)
        :owner(std::move(value)),members(std::move(input)){}
};
ALMetalWorldVisibilityView::ALMetalWorldVisibilityView(std::shared_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldVisibilityView::~ALMetalWorldVisibilityView()=default;
std::size_t ALMetalWorldVisibilityView::size() const noexcept{return mImpl->members.size();}
ALMetalWorldVisibilityArguments::ALMetalWorldVisibilityArguments(std::shared_ptr<Impl> impl,
    std::shared_ptr<const ALMetalWorldVisibilityView::Impl> view):mImpl(std::move(impl)),mView(std::move(view)){}
ALMetalWorldVisibilityArguments::~ALMetalWorldVisibilityArguments()=default;
bool ALMetalWorldVisibilityArguments::appliesTo(std::size_t index,std::uint32_t count,const float* matrix,bool depth_clamp) const noexcept
{return index<mImpl->admitted.size() && mImpl->admitted[index][1] && mImpl->admitted[index][0]==count &&
    (!matrix || std::memcmp(matrix,mView->members[index].modelview_projection,16*sizeof(float))==0) &&
    (!depth_clamp || mView->members[index].depth_clamp);}
std::uint64_t ALMetalWorldVisibilityArguments::boundsGeneration() const noexcept{return mImpl->generation;}
bool ALMetalWorldVisibilityArguments::readCompletedArgumentForTesting(std::size_t index,ALMetalWorldIndexedArguments& output) const
{
    output={};
    return index<mImpl->admitted.size() && mImpl->native_command.status==MTLCommandBufferStatusCompleted &&
        mImpl->arguments->read(index*sizeof(output),&output,sizeof(output),nullptr);
}
bool ALMetalWorldVisibilityArguments::drawIndexed(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    std::size_t index,ALMetalIndexType type,const ALMetalBuffer& indices,std::size_t offset,
    std::uint32_t count,std::string* error) const
{
    if(!appliesTo(index,count) || command.isCommitted() || command.completionToken()!=mImpl->consumer)
    {fail(error,"Visibility arguments require their original command and admitted draw");return false;}
    return encoder.drawIndexedIndirect(ALMetalPrimitiveType::triangle,type,indices,offset,
        *mImpl->arguments,index*sizeof(ALMetalWorldIndexedArguments),count,error);
}
ALMetalWorldVisibility::ALMetalWorldVisibility(std::shared_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldVisibility::~ALMetalWorldVisibility()=default;
std::shared_ptr<ALMetalWorldVisibility> ALMetalWorldVisibility::create(const ALMetalDevice& device,std::string* error)
try
{
    auto impl=std::make_shared<Impl>();impl->device=&device;
    impl->native_device=(__bridge id<MTLDevice>)device.nativeHandle();
    impl->library=ALMetalShaderLibrary::compileSource(device,alMetalWorldVisibilitySource,
        "Alchemy shared conservative world visibility",error);if(!impl->library)return {};
    id<MTLLibrary> library=(__bridge id<MTLLibrary>)impl->library->nativeHandle();
    auto function=[library newFunctionWithName:@"almetal_world_visibility_arguments"];
    impl->pipeline=[impl->native_device newComputePipelineStateWithFunction:function error:nil];
    if(!impl->pipeline){fail(error,"Could not compile shared world visibility");return {};}
    impl->free_slots.reserve(maximum_records);
    return std::shared_ptr<ALMetalWorldVisibility>(new ALMetalWorldVisibility(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

ALMetalWorldBoundsHandle ALMetalWorldVisibility::publishBounds(std::uint64_t identity,std::uint64_t revision,
    const ALMetalWorldConservativeBounds& bounds)
{
    if(!identity || !revision)return {};
    const auto found=mImpl->identities.find(identity);
    std::uint32_t slot;
    const bool existing=found!=mImpl->identities.end();
    if(existing)
    {
        slot=found->second;
        if(revision<mImpl->records[slot].revision)
        {++mImpl->statistics.stale_publications;return {slot,identity,revision};}
    }
    else
    {
        if(mImpl->free_slots.empty() && mImpl->records.size()>=maximum_records)return {};
        const bool fresh=mImpl->free_slots.empty();
        slot=fresh?static_cast<std::uint32_t>(mImpl->records.size()):mImpl->free_slots.back();
        // Stage all allocating containers before changing an existing slot.
        if(fresh)
        {
            mImpl->records.emplace_back();
            try{mImpl->dirty.push_back(false);}catch(...){mImpl->records.pop_back();throw;}
        }
        try{mImpl->identities.emplace(identity,slot);}
        catch(...){if(fresh){mImpl->records.pop_back();mImpl->dirty.pop_back();}throw;}
        if(!fresh)mImpl->free_slots.pop_back();
    }
    BoundsRecord value;std::memcpy(value.minimum,bounds.minimum,sizeof(value.minimum));
    std::memcpy(value.maximum,bounds.maximum,sizeof(value.maximum));value.identity=identity;value.revision=revision;
    bool qualified=bounds.qualified;
    for(unsigned axis=0;axis<3;++axis)
        qualified=qualified && std::isfinite(value.minimum[axis]) && std::isfinite(value.maximum[axis]) && value.minimum[axis]<=value.maximum[axis];
    value.config[0]=qualified;
    if(existing && revision==mImpl->records[slot].revision &&
        std::memcmp(&mImpl->records[slot],&value,sizeof(value))!=0)
    {
        // One producer revision denotes one immutable bounds tuple. A
        // conflicting same-revision publication must never shrink an older
        // view's conservative extent while retaining its qualification key.
        ++mImpl->statistics.stale_publications;
        return {};
    }
    if(std::memcmp(&mImpl->records[slot],&value,sizeof(value))!=0)
    {mImpl->records[slot]=value;mImpl->dirty[slot]=true;mImpl->has_dirty=true;++mImpl->statistics.bounds_publications;}
    return {slot,identity,revision};
}
bool ALMetalWorldVisibility::retire(ALMetalWorldBoundsHandle handle) noexcept
{
    if(handle.slot>=mImpl->records.size())return false;
    auto& record=mImpl->records[handle.slot];
    if(record.identity!=handle.identity || record.revision!=handle.revision)return false;
    mImpl->identities.erase(handle.identity);record={};mImpl->dirty[handle.slot]=true;mImpl->has_dirty=true;
    mImpl->free_slots.push_back(handle.slot);return true;
}
std::shared_ptr<const ALMetalWorldVisibilityView> ALMetalWorldVisibility::makeView(
    std::vector<ALMetalWorldVisibilityMember> members,std::string* error)
try
{
    if(members.size()>maximum_records){fail(error,"Visibility view exceeds bounded membership storage");return {};}
    for(auto& member:members){member.enabled=member.enabled!=0;member.depth_clamp=member.depth_clamp!=0;}
    auto impl=std::make_shared<ALMetalWorldVisibilityView::Impl>(mImpl,std::move(members));
    return std::shared_ptr<const ALMetalWorldVisibilityView>(new ALMetalWorldVisibilityView(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

std::shared_ptr<ALMetalWorldVisibilityArguments> ALMetalWorldVisibility::encode(
    ALMetalCommandBuffer& command,const ALMetalWorldVisibilityView& view,std::string* error)
try
{
    auto& owner=*view.mImpl->owner;const auto& members=view.mImpl->members;
    const auto reject=[&](const char* text,ALMetalWorldVisibilityFailure reason=ALMetalWorldVisibilityFailure::invalid_input)->std::shared_ptr<ALMetalWorldVisibilityArguments>
    {fallback(owner.statistics,reason);fail(error,text);return {};};
    if(command.isCommitted() || command.nativeDevice()!=(__bridge void*)owner.native_device || members.empty())
        return reject("Shared visibility requires an uncommitted matching command and members");
    auto completion=command.completionToken();if(!completion)return reject("Shared visibility requires completion tracking",ALMetalWorldVisibilityFailure::completion_tracking);
    if(owner.current)
        std::erase_if(owner.current->readers,[](const auto& reader){return reader->isComplete();});
    if(!owner.current || owner.has_dirty)
    {
        std::size_t capacity=64;
        while(capacity<owner.records.size())capacity*=2;
        const auto length=capacity*sizeof(BoundsRecord);
        const bool reuse=owner.current && owner.current->complete() && owner.current->buffer->length()>=length;
        auto generation=reuse?owner.current:std::make_shared<BoundsGeneration>();
        if(!reuse)
        {
            ALMetalBufferDescriptor descriptor;descriptor.length=length;descriptor.storage=ALMetalStorageMode::shared;
            descriptor.label="Alchemy persistent conservative bounds generation";
            generation->buffer=ALMetalBuffer::create(*owner.device,descriptor,error);
            if(!generation->buffer){fallback(owner.statistics,ALMetalWorldVisibilityFailure::bounds_allocation);return {};}
        }
        auto* mapped=static_cast<std::byte*>(generation->buffer->mappedBytes());
        if(!mapped)return reject("Shared bounds generation is not CPU mapped",ALMetalWorldVisibilityFailure::publication);
        if(reuse)
        {
            for(std::size_t slot=0;slot<owner.records.size();++slot)if(owner.dirty[slot])
            {
                std::memcpy(mapped+slot*sizeof(BoundsRecord),&owner.records[slot],sizeof(BoundsRecord));
                ++owner.statistics.dirty_records_uploaded;owner.statistics.uploaded_bytes+=sizeof(BoundsRecord);
            }
            ++owner.statistics.generation_reuses;
        }
        else
        {
            if(!owner.records.empty())std::memcpy(mapped,owner.records.data(),owner.records.size()*sizeof(BoundsRecord));
            owner.statistics.uploaded_bytes+=owner.records.size()*sizeof(BoundsRecord);
            owner.statistics.dirty_records_uploaded+=owner.records.size();++owner.statistics.generation_copies;
        }
        generation->readers.clear();generation->serial=++owner.next_generation;
        owner.current=std::move(generation);std::fill(owner.dirty.begin(),owner.dirty.end(),false);owner.has_dirty=false;
    }
    std::shared_ptr<ALMetalWorldVisibilityArguments::Impl> slot;
    for(const auto& candidate:owner.slots)
        if(candidate.use_count()==1 && (!candidate->consumer || candidate->consumer->isComplete()))
        {candidate->bounds.reset();slot=candidate;break;}
    if(!slot)
    {
        if(owner.slots.size()>=maximum_argument_slots)return reject("Shared visibility argument pool is busy",ALMetalWorldVisibilityFailure::arguments_busy);
        slot=std::make_shared<ALMetalWorldVisibilityArguments::Impl>();owner.slots.push_back(slot);
    }
    const auto ensure=[&](std::unique_ptr<ALMetalBuffer>& buffer,std::size_t bytes,const char* label)
    {
        if(buffer && buffer->length()>=bytes)return true;
        std::size_t capacity=4096;while(capacity<bytes)capacity*=2;
        ALMetalBufferDescriptor descriptor;descriptor.length=capacity;descriptor.storage=ALMetalStorageMode::shared;
        descriptor.label=label;buffer=ALMetalBuffer::create(*owner.device,descriptor,error);return bool(buffer);
    };
    if(!ensure(slot->members,members.size()*sizeof(ALMetalWorldVisibilityMember),"Alchemy ordered visibility membership") ||
       !ensure(slot->arguments,members.size()*sizeof(ALMetalWorldIndexedArguments),"Alchemy ordered indexed draw arguments"))
    {fallback(owner.statistics,ALMetalWorldVisibilityFailure::arguments_allocation);return {};}
    if(!slot->members->write(0,members.data(),members.size()*sizeof(ALMetalWorldVisibilityMember),error))
    {fallback(owner.statistics,ALMetalWorldVisibilityFailure::publication);return {};}
    slot->admitted.resize(members.size());
    std::uint64_t candidates=0;
    for(std::size_t index=0;index<members.size();++index)
    {slot->admitted[index]={members[index].index_count,members[index].enabled};candidates+=members[index].enabled!=0;}
    slot->bounds=owner.current;slot->consumer=completion;slot->generation=owner.current->serial;
    slot->native_command=(__bridge id<MTLCommandBuffer>)command.nativeHandle();
    auto result=std::shared_ptr<ALMetalWorldVisibilityArguments>(new ALMetalWorldVisibilityArguments(slot,view.mImpl));
    owner.current->readers.push_back(completion);
    if(!command.retainUntilCompletion(slot,error)){fallback(owner.statistics,ALMetalWorldVisibilityFailure::retention);return {};}
    auto encoder=[slot->native_command computeCommandEncoder];
    if(!encoder)return reject("Could not begin shared visibility compute",ALMetalWorldVisibilityFailure::encoder);
    [encoder setComputePipelineState:owner.pipeline];
    [encoder setBuffer:(__bridge id<MTLBuffer>)owner.current->buffer->nativeHandle() offset:0 atIndex:0];
    [encoder setBuffer:(__bridge id<MTLBuffer>)slot->members->nativeHandle() offset:0 atIndex:1];
    [encoder setBuffer:(__bridge id<MTLBuffer>)slot->arguments->nativeHandle() offset:0 atIndex:2];
    const std::array<std::uint32_t,2> counts{static_cast<std::uint32_t>(members.size()),static_cast<std::uint32_t>(owner.records.size())};
    [encoder setBytes:counts.data() length:sizeof(counts) atIndex:3];
    [encoder dispatchThreads:MTLSizeMake(members.size(),1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
    [encoder endEncoding];++owner.statistics.views;owner.statistics.argument_candidates+=candidates;
    return result;
}
catch(const std::bad_alloc&)
{
    fallback(view.mImpl->owner->statistics,ALMetalWorldVisibilityFailure::host_allocation);
    if(error)error->clear();return {};
}
ALMetalWorldVisibilityStats ALMetalWorldVisibility::stats() const noexcept{return mImpl->statistics;}
