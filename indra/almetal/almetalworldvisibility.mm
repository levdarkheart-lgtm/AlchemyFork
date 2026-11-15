/** Shared persistent conservative bounds and ordered same-command arguments. */
#include "almetalworldvisibility.h"
#include "almetalworld.h"
#include "almetalworldvisibilitysource.h"
#include "almetalworldcohort.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <new>
#include <unordered_map>
#include <map>
#include <atomic>
#include <cstddef>

ALMetalWorldCpuVisibilityResult alMetalClassifyWorldBoundsCPU(
    const ALMetalWorldConservativeBounds& bounds,const float* matrix) noexcept
{
    using Result=ALMetalWorldCpuVisibilityResult;
    // Bit tests survive the renderer's fast-math finite assumptions. Reject
    // subnormal inputs too: GPU input flushing could otherwise be amplified
    // by a large finite matrix coefficient before the dot-product error bound.
    const auto ordinary=[](float value) noexcept
    {
        const auto bits=std::bit_cast<std::uint32_t>(value)&0x7fffffffu;
        return bits<0x7f800000u && (bits==0 || bits>=0x00800000u);
    };
    if(!bounds.qualified || !matrix)return Result::unknown;
    for(unsigned i=0;i<16;++i)if(!ordinary(matrix[i]))return Result::unknown;
    for(unsigned axis=0;axis<4;++axis)
        if(!ordinary(bounds.minimum[axis]) || !ordinary(bounds.maximum[axis]) ||
           bounds.minimum[axis]>bounds.maximum[axis])return Result::unknown;

    // Evaluate affine support intervals rather than transforming eight
    // corners. Double is only the interval evaluator: this error still bounds
    // the actual FLOAT products/additions, different contraction orders,
    // native clip-Z conversion and flush-to-zero of intermediate products.
    constexpr double float_error=64.0*0x1p-24;
    constexpr double flush_error=16.0*FLT_MIN;
    double center[3]{},extent[3]{},absolute[3]{};
    for(unsigned axis=0;axis<3;++axis)
    {
        const double lower=bounds.minimum[axis],upper=bounds.maximum[axis];
        center[axis]=(lower+upper)*.5;extent[axis]=(upper-lower)*.5;
        absolute[axis]=std::max(std::abs(lower),std::abs(upper));
    }
    double clip[4]{},radius[4]{},error[4]{};
    for(unsigned component=0;component<4;++component)
    {
        clip[component]=matrix[12+component];
        double magnitude=std::abs(clip[component]);
        for(unsigned axis=0;axis<3;++axis)
        {
            const double coefficient=matrix[axis*4+component];
            clip[component]+=coefficient*center[axis];
            radius[component]+=std::abs(coefficient)*extent[axis];
            magnitude+=std::abs(coefficient)*absolute[axis];
        }
        error[component]=float_error*magnitude+flush_error;
        // Minimum absolute value over the entire affine interval is no
        // greater than at any corner. This can decline more ill-conditioned
        // boxes, but cannot relax the previous cancellation/overflow guard.
        const double minimum_absolute=std::max(0.0,std::abs(clip[component])-radius[component]);
        if(magnitude>static_cast<double>(FLT_MAX)/8.0 ||
           error[component]>std::max(1.0,minimum_absolute)/4096.0)
            return Result::unknown;
    }
    if(clip[3]-radius[3]<=error[3])return Result::unknown;
    bool wholly_inside=true;
    for(unsigned axis=0;axis<3;++axis)
    {
        const double uncertainty=error[axis]+error[3];
        for(double sign:{1.0,-1.0})
        {
            // Combine coefficients BEFORE taking absolute values. Adding
            // independent X/W radii would discard their shared-position
            // correlation and needlessly weaken the clip-plane test.
            const double distance=clip[3]+sign*clip[axis];
            double plane_radius=0;
            for(unsigned coordinate=0;coordinate<3;++coordinate)
                plane_radius+=std::abs(static_cast<double>(matrix[coordinate*4+3])+
                    sign*matrix[coordinate*4+axis])*extent[coordinate];
            if(distance+plane_radius < -uncertainty)return Result::outside;
            wholly_inside=wholly_inside && distance-plane_radius>uncertainty;
        }
    }
    return wholly_inside?Result::inside:Result::partial;
}

namespace
{
constexpr std::size_t maximum_records=65536, maximum_argument_slots=32;
constexpr std::size_t maximum_cached_cohorts=2048, maximum_cohort_row_bytes=8*1024*1024;
constexpr std::size_t maximum_cohort_descriptor_bytes=8*1024*1024;
struct alignas(16) CohortRecord
{
    float minimum[4]{},maximum[4]{};
    std::uint32_t config[4]{}; // index count, qualification; never viewer pointers.
};
static_assert(sizeof(CohortRecord)==48);
// Tier-2 GPU addresses, not CPU pointers. Keep these offsets identical to
// CohortDispatch in the visibility shader; each prefix covers nonempty rows.
struct alignas(16) CohortDispatch
{
    std::uint64_t rows_address=0,reserved=0;
    float matrix[16]{};
    std::uint32_t config[4]{}; // count, logical destination, packed prefix
};
static_assert(sizeof(CohortDispatch)==96);
static_assert(offsetof(CohortDispatch,matrix)==16);
static_assert(offsetof(CohortDispatch,config)==80);
struct CohortDescriptors
{
    std::unique_ptr<ALMetalBuffer> buffer;
    std::shared_ptr<std::atomic<std::size_t>> live;
    std::size_t bytes=0;
    ~CohortDescriptors(){if(live)live->fetch_sub(bytes,std::memory_order_relaxed);}
};
struct CohortRows
{
    std::unique_ptr<ALMetalBuffer> buffer;
    std::shared_ptr<std::atomic<std::size_t>> live;
    std::size_t bytes=0,count=0,qualified=0;
    ~CohortRows(){if(live)live->fetch_sub(bytes,std::memory_order_relaxed);}
};
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
    std::unique_ptr<ALMetalBuffer> destinations;
    std::vector<std::shared_ptr<CohortRows>> cohort_rows;
    std::unique_ptr<CohortDescriptors> cohort_descriptors;
    std::uint64_t generation=0;
};
struct ALMetalWorldVisibility::Impl
{
    const ALMetalDevice* device=nullptr;
    id<MTLDevice> native_device=nil;
    std::unique_ptr<ALMetalShaderLibrary> library;
    id<MTLComputePipelineState> pipeline=nil;
    id<MTLComputePipelineState> cohort_pipeline=nil;
    id<MTLComputePipelineState> cohort_batch_pipeline=nil;
    std::vector<BoundsRecord> records;
    std::vector<bool> dirty;
    std::vector<std::uint32_t> free_slots;
    std::unordered_map<std::uint64_t,std::uint32_t> identities;
    bool has_dirty=false;
    std::shared_ptr<BoundsGeneration> current;
    std::vector<std::shared_ptr<ALMetalWorldVisibilityArguments::Impl>> slots;
    ALMetalWorldVisibilityStats statistics;
    std::uint64_t next_generation=0;
    std::uint64_t producer_epoch=0;
    struct CohortEntry {const void* pointer=nullptr;std::shared_ptr<CohortRows> rows;};
    std::map<std::weak_ptr<const ALMetalWorldSourceCohort>,CohortEntry,
        std::owner_less<std::weak_ptr<const ALMetalWorldSourceCohort>>> cohort_cache;
    std::shared_ptr<std::atomic<std::size_t>> live_cohort_bytes=std::make_shared<std::atomic<std::size_t>>(0);
    std::shared_ptr<std::atomic<std::size_t>> live_descriptor_bytes=std::make_shared<std::atomic<std::size_t>>(0);
};
struct ALMetalWorldVisibilityView::Impl
{
    std::shared_ptr<ALMetalWorldVisibility::Impl> owner;
    const std::vector<ALMetalWorldVisibilityMember> members;
    const std::vector<ALMetalWorldSourceCohortView> cohorts;
    const std::vector<std::uint32_t> logical_indices;
    const std::size_t logical_count;
    const bool sparse=false;
    Impl(std::shared_ptr<ALMetalWorldVisibility::Impl> value,std::vector<ALMetalWorldVisibilityMember> input)
        :owner(std::move(value)),members(std::move(input)),logical_count(members.size()){}
    Impl(std::shared_ptr<ALMetalWorldVisibility::Impl> value,std::vector<ALMetalWorldVisibilityMember> input,
        std::span<const ALMetalWorldSourceCohortView> groups,std::size_t count)
        :owner(std::move(value)),members(std::move(input)),cohorts(groups.begin(),groups.end()),logical_count(count){}
    Impl(std::shared_ptr<ALMetalWorldVisibility::Impl> value,std::vector<ALMetalWorldVisibilityMember> input,
        std::vector<std::uint32_t> indices,std::vector<ALMetalWorldSourceCohortView> groups,std::size_t count)
        :owner(std::move(value)),members(std::move(input)),cohorts(std::move(groups)),
         logical_indices(std::move(indices)),logical_count(count),sparse(true){}
    bool hasArgument(std::size_t index) const noexcept
    {
        if(index>=logical_count)return false;if(!sparse)return true;
        if(std::binary_search(logical_indices.begin(),logical_indices.end(),index))return true;
        const auto after=std::upper_bound(cohorts.begin(),cohorts.end(),index,
            [](std::size_t value,const auto& cohort){return value<cohort.logical_begin;});
        return after!=cohorts.begin() && index-std::prev(after)->logical_begin<std::prev(after)->cohort->size();
    }
    bool appliesTo(std::size_t index,std::uint32_t count,const float* matrix,bool depth_clamp,bool allow_cohort=false) const noexcept
    {
        if(index>=logical_count)return false;
        auto after=std::upper_bound(cohorts.begin(),cohorts.end(),index,
            [](std::size_t value,const auto& cohort){return value<cohort.logical_begin;});
        std::size_t before=0;
        if(after!=cohorts.begin())
        {
            const auto& cohort=*std::prev(after);const auto local=index-cohort.logical_begin;
            if(local<cohort.cohort->size())
                return allow_cohort && cohort.cohort->source(local).index_count==count && cohort.cohort->bounds(local).qualified &&
                    !depth_clamp && (!matrix || std::memcmp(matrix,cohort.view.modelview_projection,16*sizeof(float))==0);
            before=cohort.logical_begin-cohort.base_before+cohort.cohort->size();
        }
        auto base=index-before;
        if(sparse)
        {
            const auto found=std::lower_bound(logical_indices.begin(),logical_indices.end(),index);
            if(found==logical_indices.end() || *found!=index)return false;
            base=found-logical_indices.begin();
        }
        return base<members.size() && members[base].enabled && members[base].index_count==count &&
            (!matrix || std::memcmp(matrix,members[base].modelview_projection,16*sizeof(float))==0) &&
            (!depth_clamp || members[base].depth_clamp);
    }
};
ALMetalWorldVisibilityView::ALMetalWorldVisibilityView(std::shared_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalWorldVisibilityView::~ALMetalWorldVisibilityView()=default;
std::size_t ALMetalWorldVisibilityView::size() const noexcept{return mImpl->logical_count;}
ALMetalWorldVisibilityArguments::ALMetalWorldVisibilityArguments(std::shared_ptr<Impl> impl,
    std::shared_ptr<const ALMetalWorldVisibilityView::Impl> view):mImpl(std::move(impl)),mView(std::move(view)){}
ALMetalWorldVisibilityArguments::~ALMetalWorldVisibilityArguments()=default;
bool ALMetalWorldVisibilityArguments::appliesTo(std::size_t index,std::uint32_t count,const float* matrix,bool depth_clamp) const noexcept
{return mView->appliesTo(index,count,matrix,depth_clamp);}
bool ALMetalWorldVisibilityArguments::appliesToCohort(std::size_t index,std::uint32_t count,
    const ALMetalWorldSourceCohortView& current) const noexcept
{
    const auto after=std::upper_bound(mView->cohorts.begin(),mView->cohorts.end(),index,
        [](std::size_t value,const auto& cohort){return value<cohort.logical_begin;});
    if(after==mView->cohorts.begin())return false;
    const auto& expected=*std::prev(after);
    const auto exact=[](const auto& a,const auto& b)
    {return a && b && a.get()==b.get() && !a.owner_before(b) && !b.owner_before(a);};
    return current.logical_begin==expected.logical_begin && current.base_before==expected.base_before &&
        exact(current.cohort,expected.cohort) && exact(current.cpu_owner,expected.cpu_owner) &&
        exact(current.native_owner,expected.native_owner) &&
        index-current.logical_begin<current.cohort->size() &&
        mView->appliesTo(index,count,current.view.modelview_projection,false,true);
}
std::uint64_t ALMetalWorldVisibilityArguments::boundsGeneration() const noexcept{return mImpl->generation;}
bool ALMetalWorldVisibilityArguments::readCompletedArgumentForTesting(std::size_t index,ALMetalWorldIndexedArguments& output) const
{
    output={};
    return mView->hasArgument(index) && mImpl->native_command.status==MTLCommandBufferStatusCompleted &&
        mImpl->arguments->read(index*sizeof(output),&output,sizeof(output),nullptr);
}
bool ALMetalWorldVisibilityArguments::drawIndexed(ALMetalRenderEncoder& encoder,ALMetalCommandBuffer& command,
    std::size_t index,ALMetalIndexType type,const ALMetalBuffer& indices,std::size_t offset,
    std::uint32_t count,std::string* error) const
{
    if(!mView->appliesTo(index,count,nullptr,false,true) || command.isCommitted() || command.completionToken()!=mImpl->consumer)
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
    auto cohort_function=[library newFunctionWithName:@"almetal_world_visibility_cohort_arguments"];
    impl->cohort_pipeline=[impl->native_device newComputePipelineStateWithFunction:cohort_function error:nil];
    if(!impl->cohort_pipeline){fail(error,"Could not compile retained cohort visibility");return {};}
    // The old direct-buffer kernels remain available without this optional
    // pointer-descriptor capability. Multi-cohort requests fail to direct
    // rendering when the exact native support cannot be proved.
    if(impl->native_device.argumentBuffersSupport==MTLArgumentBuffersTier2)
    {
        auto batch_function=[library newFunctionWithName:@"almetal_world_visibility_cohorts_arguments"];
        if(batch_function)impl->cohort_batch_pipeline=[impl->native_device newComputePipelineStateWithFunction:batch_function error:nil];
    }
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
    {mImpl->records[slot]=value;mImpl->dirty[slot]=true;mImpl->has_dirty=true;++mImpl->statistics.bounds_publications;++mImpl->producer_epoch;}
    return {slot,identity,revision};
}
bool ALMetalWorldVisibility::retire(ALMetalWorldBoundsHandle handle) noexcept
{
    if(handle.slot>=mImpl->records.size())return false;
    auto& record=mImpl->records[handle.slot];
    if(record.identity!=handle.identity || record.revision!=handle.revision)return false;
    mImpl->identities.erase(handle.identity);record={};mImpl->dirty[handle.slot]=true;mImpl->has_dirty=true;++mImpl->producer_epoch;
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

std::shared_ptr<const ALMetalWorldVisibilityView> ALMetalWorldVisibility::makeView(
    std::vector<ALMetalWorldVisibilityMember> members,std::span<const ALMetalWorldSourceCohortView> cohorts,
    std::size_t logical_count,std::string* error)
try
{
    if(logical_count>maximum_records || members.size()>logical_count)
    {fail(error,"Retained visibility view exceeds bounded membership storage");return {};}
    std::size_t preceding=0,previous_base=0;
    for(const auto& group:cohorts)
    {
        if(!group.cohort || !group.cohort->size() || group.base_before<previous_base ||
           group.base_before>members.size() || group.cohort->size()>logical_count-preceding ||
           group.logical_begin!=group.base_before+preceding ||
           !group.cohort->matchesOwners(group.cpu_owner,group.native_owner) ||
           !ALMetalWorldSourceCohort::validView(group.view))
        {fail(error,"Retained visibility requires exact ordered completed cohort owners");return {};}
        preceding+=group.cohort->size();previous_base=group.base_before;
    }
    if(preceding!=logical_count-members.size())
    {fail(error,"Retained visibility logical membership is incomplete");return {};}
    for(auto& member:members){member.enabled=member.enabled!=0;member.depth_clamp=member.depth_clamp!=0;}
    auto impl=std::make_shared<ALMetalWorldVisibilityView::Impl>(mImpl,std::move(members),cohorts,logical_count);
    return std::shared_ptr<const ALMetalWorldVisibilityView>(new ALMetalWorldVisibilityView(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

std::shared_ptr<const ALMetalWorldVisibilityView> ALMetalWorldVisibility::makeView(
    std::vector<ALMetalWorldVisibilityMember> members,std::vector<std::uint32_t> indices,
    std::span<const ALMetalWorldSourceCohortView> cohorts,std::size_t logical_count,std::string* error)
try
{
    if(logical_count>maximum_records || members.size()!=indices.size() || members.size()>logical_count || cohorts.size()>logical_count)
    {fail(error,"Sparse visibility membership is incomplete or exceeds storage");return {};}
    std::size_t previous_end=0;
    std::vector<ALMetalWorldSourceCohortView> qualified;qualified.reserve(cohorts.size());
    for(const auto& group:cohorts)
    {
        if(!group.cohort || !group.cohort->size() || group.logical_begin<previous_end ||
           group.logical_begin>logical_count || group.cohort->size()>logical_count-group.logical_begin ||
           group.base_before>group.logical_begin || !group.cohort->matchesOwners(group.cpu_owner,group.native_owner) ||
           !ALMetalWorldSourceCohort::validView(group.view))
        {fail(error,"Sparse visibility cohort ownership or logical range is invalid");return {};}
        previous_end=group.logical_begin+group.cohort->size();
        if(group.cohort->qualifiedBoundsCount())qualified.push_back(group);
    }
    std::size_t cohort=0;std::vector<ALMetalWorldVisibilityMember> selected;std::vector<std::uint32_t> selected_indices;
    selected.reserve(members.size());selected_indices.reserve(indices.size());
    for(std::size_t raw=0;raw<indices.size();++raw)
    {
        const auto index=indices[raw];
        while(cohort<cohorts.size() && cohorts[cohort].logical_begin+cohorts[cohort].cohort->size()<=index)++cohort;
        if(index>=logical_count || (raw && index<=indices[raw-1]) ||
           (cohort<cohorts.size() && index>=cohorts[cohort].logical_begin))
        {fail(error,"Sparse visibility raw indices overlap or are out of order");return {};}
        if(members[raw].enabled)
        {members[raw].enabled=1;members[raw].depth_clamp=members[raw].depth_clamp!=0;
         selected.push_back(members[raw]);selected_indices.push_back(index);}
    }
    if(selected.empty() && qualified.empty()){if(error)error->clear();return {};}
    auto impl=std::make_shared<ALMetalWorldVisibilityView::Impl>(mImpl,std::move(selected),std::move(selected_indices),std::move(qualified),logical_count);
    return std::shared_ptr<const ALMetalWorldVisibilityView>(new ALMetalWorldVisibilityView(std::move(impl)));
}
catch(const std::bad_alloc&){if(error)error->clear();return {};}

std::shared_ptr<ALMetalWorldVisibilityArguments> ALMetalWorldVisibility::encode(
    ALMetalCommandBuffer& command,const ALMetalWorldVisibilityView& view,std::string* error,ALMetalWorldFrameData* frame_data)
try
{
    auto& owner=*view.mImpl->owner;const auto& members=view.mImpl->members;
    const auto& cohorts=view.mImpl->cohorts;const auto logical_count=view.mImpl->logical_count;
    const auto reject=[&](const char* text,ALMetalWorldVisibilityFailure reason=ALMetalWorldVisibilityFailure::invalid_input)->std::shared_ptr<ALMetalWorldVisibilityArguments>
    {fallback(owner.statistics,reason);fail(error,text);return {};};
    if(command.isCommitted() || command.nativeDevice()!=(__bridge void*)owner.native_device || !logical_count)
        return reject("Shared visibility requires an uncommitted matching command and members");
    const bool batched=cohorts.size()>1;
    if(batched && !owner.cohort_batch_pipeline)
        return reject("Packed cohort visibility requires native Tier-2 GPU addresses",ALMetalWorldVisibilityFailure::cohort_batch_unsupported);
    auto completion=command.completionToken();if(!completion)return reject("Shared visibility requires completion tracking",ALMetalWorldVisibilityFailure::completion_tracking);
    std::shared_ptr<ALMetalWorldVisibilityArguments::Impl> slot;
    for(const auto& candidate:owner.slots)
        if(candidate.use_count()==1 && (!candidate->consumer || candidate->consumer->isComplete()))
        {candidate->bounds.reset();candidate->cohort_rows.clear();slot=candidate;break;}
    if(!slot)
    {
        if(owner.slots.size()>=maximum_argument_slots)return reject("Shared visibility argument pool is busy",ALMetalWorldVisibilityFailure::arguments_busy);
        slot=std::make_shared<ALMetalWorldVisibilityArguments::Impl>();owner.slots.push_back(slot);
    }
    if(owner.current)
        std::erase_if(owner.current->readers,[](const auto& reader){return reader->isComplete();});
    for(auto it=owner.cohort_cache.begin();it!=owner.cohort_cache.end();)
        if(it->first.expired())it=owner.cohort_cache.erase(it);else ++it;
    const bool raw_bounds=!members.empty() && !owner.records.empty();
    ALMetalWorldSourcePublicationWork work;
    std::size_t new_cohort_bytes=0;
    if(raw_bounds && (!owner.current || owner.has_dirty))
    {
        std::size_t capacity=64;while(capacity<owner.records.size())capacity*=2;
        const auto length=capacity*sizeof(BoundsRecord);
        const bool reuse=owner.current && owner.current->complete() && owner.current->buffer->length()>=length;
        work.encoded_row_bytes=(reuse?std::count(owner.dirty.begin(),owner.dirty.end(),true):owner.records.size())*sizeof(BoundsRecord);
        if(!reuse)work.allocation_bytes=length;
    }
    for(const auto& group:cohorts)
    {
        const auto cached=owner.cohort_cache.find(group.cohort);
        if(cached==owner.cohort_cache.end() || cached->second.pointer!=group.cohort.get())
        {
            // Conservative upper bound if one immutable cohort occurs more
            // than once in the view; no per-source request/key array is built.
            const auto bytes=group.cohort->size()*sizeof(CohortRecord);
            work.encoded_row_bytes+=bytes;work.allocation_bytes+=bytes;
            new_cohort_bytes+=bytes;
        }
    }
    const auto retained=owner.live_cohort_bytes->load(std::memory_order_relaxed);
    if(retained>maximum_cohort_row_bytes || new_cohort_bytes>maximum_cohort_row_bytes-retained)
        return reject("Retained visibility native row capacity is exhausted",ALMetalWorldVisibilityFailure::bounds_allocation);
    const auto epoch=owner.producer_epoch;std::shared_ptr<const void> publication_credit;
    if(frame_data && work.chargedBytes())
    {
        if(!frame_data->requestSourcePublication(work,publication_credit))
        {
            ++owner.statistics.admission_requests;++owner.statistics.admission_denials;
            return reject("Native visibility publication was deferred",ALMetalWorldVisibilityFailure::admission);
        }
        if(publication_credit)++owner.statistics.admission_requests;
        if(owner.producer_epoch!=epoch || command.isCommitted() || command.completionToken()!=completion)
            return reject("Native visibility publication plan changed during admission",ALMetalWorldVisibilityFailure::stale_plan);
        if(publication_credit)
        {
            if(!command.retainUntilCompletion(publication_credit,error))
                return reject("Native visibility publication credit retention failed",ALMetalWorldVisibilityFailure::retention);
            owner.statistics.admitted_logical_bytes+=work.chargedBytes();
        }
    }
    if(raw_bounds && (!owner.current || owner.has_dirty))
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
    const auto ensure=[&](std::unique_ptr<ALMetalBuffer>& buffer,std::size_t bytes,const char* label)
    {
        if(buffer && buffer->length()>=bytes)return true;
        std::size_t capacity=4096;while(capacity<bytes)capacity*=2;
        ALMetalBufferDescriptor descriptor;descriptor.length=capacity;descriptor.storage=ALMetalStorageMode::shared;
        descriptor.label=label;buffer=ALMetalBuffer::create(*owner.device,descriptor,error);return bool(buffer);
    };
    if((!members.empty() && !ensure(slot->members,members.size()*sizeof(ALMetalWorldVisibilityMember),"Alchemy ordered visibility membership")) ||
       !ensure(slot->arguments,logical_count*sizeof(ALMetalWorldIndexedArguments),"Alchemy ordered indexed draw arguments"))
    {fallback(owner.statistics,ALMetalWorldVisibilityFailure::arguments_allocation);return {};}
    if(!members.empty() && !slot->members->write(0,members.data(),members.size()*sizeof(ALMetalWorldVisibilityMember),error))
    {fallback(owner.statistics,ALMetalWorldVisibilityFailure::publication);return {};}
    slot->admitted.resize(members.size());
    std::uint64_t candidates=0;
    for(std::size_t index=0;index<members.size();++index)
    {slot->admitted[index]={members[index].index_count,members[index].enabled};candidates+=members[index].enabled!=0;}
    // Only ordinary tokens need a per-view mapping. Cohort membership and
    // conservative bounds are uploaded once for their private generation.
    const bool mapped_destinations=view.mImpl->sparse || !cohorts.empty();
    if(!members.empty() && mapped_destinations)
    {
        std::vector<std::uint32_t> destinations;
        const std::uint32_t* data=view.mImpl->logical_indices.data();
        if(!view.mImpl->sparse)
        {
            destinations.reserve(members.size());std::size_t group=0,preceding=0;
            for(std::size_t base=0;base<members.size();++base)
            {
                while(group<cohorts.size() && cohorts[group].base_before<=base)preceding+=cohorts[group++].cohort->size();
                destinations.push_back(static_cast<std::uint32_t>(base+preceding));
            }
            data=destinations.data();
        }
        if(!ensure(slot->destinations,members.size()*sizeof(std::uint32_t),"Alchemy ordinary visibility destinations") ||
           !slot->destinations->write(0,data,members.size()*sizeof(std::uint32_t),error))
            return reject("Could not publish ordinary visibility destinations",ALMetalWorldVisibilityFailure::publication);
    }
    slot->cohort_rows.reserve(cohorts.size());
    std::uint64_t cohort_candidates=0;
    for(const auto& group:cohorts)
    {
        auto cached=owner.cohort_cache.find(group.cohort);
        std::shared_ptr<CohortRows> rows;
        if(cached!=owner.cohort_cache.end() && cached->second.pointer==group.cohort.get())
        {rows=cached->second.rows;++owner.statistics.cohort_row_hits;}
        else
        {
            const auto bytes=group.cohort->size()*sizeof(CohortRecord);
            const auto live=owner.live_cohort_bytes->load(std::memory_order_relaxed);
            if(live>maximum_cohort_row_bytes || bytes>maximum_cohort_row_bytes-live)
                return reject("Retained visibility native row capacity is exhausted",ALMetalWorldVisibilityFailure::bounds_allocation);
            rows=std::make_shared<CohortRows>();rows->count=group.cohort->size();
            ALMetalBufferDescriptor descriptor;descriptor.length=bytes;descriptor.storage=ALMetalStorageMode::shared;
            descriptor.label="Alchemy immutable cohort conservative rows";
            rows->buffer=ALMetalBuffer::create(*owner.device,descriptor,error);
            if(!rows->buffer)return reject("Could not allocate retained visibility rows",ALMetalWorldVisibilityFailure::bounds_allocation);
            const auto allocated=[(__bridge id<MTLBuffer>)rows->buffer->nativeHandle() allocatedSize];
            const auto retained=owner.live_cohort_bytes->load(std::memory_order_relaxed);
            if(!allocated || allocated<bytes || retained>maximum_cohort_row_bytes || allocated>maximum_cohort_row_bytes-retained)
                return reject("Retained visibility physical row capacity is exhausted",ALMetalWorldVisibilityFailure::bounds_allocation);
            rows->live=owner.live_cohort_bytes;rows->bytes=allocated;
            rows->live->fetch_add(allocated,std::memory_order_relaxed);
            auto* mapped=static_cast<CohortRecord*>(rows->buffer->mappedBytes());
            if(!mapped)return reject("Retained visibility rows are not mapped",ALMetalWorldVisibilityFailure::publication);
            for(std::size_t source=0;source<rows->count;++source)
            {
                const auto& bounds=group.cohort->bounds(source);CohortRecord row;
                std::memcpy(row.minimum,bounds.minimum,sizeof(row.minimum));std::memcpy(row.maximum,bounds.maximum,sizeof(row.maximum));
                row.config[0]=group.cohort->source(source).index_count;row.config[1]=bounds.qualified;
                rows->qualified+=bounds.qualified;mapped[source]=row;
            }
            ++owner.statistics.cohort_row_builds;owner.statistics.cohort_row_upload_bytes+=bytes;
            owner.statistics.cohort_row_allocated_bytes+=allocated;
            if(cached==owner.cohort_cache.end() && owner.cohort_cache.size()<maximum_cached_cohorts)
                owner.cohort_cache.emplace(group.cohort,Impl::CohortEntry{group.cohort.get(),rows});
        }
        cohort_candidates+=rows->qualified;slot->cohort_rows.push_back(std::move(rows));
    }
    std::vector<void*> indirect_rows;
    std::size_t packed_count=0;
    if(batched)
    {
        std::vector<CohortDispatch> descriptors(cohorts.size());indirect_rows.reserve(cohorts.size());
        for(std::size_t index=0;index<cohorts.size();++index)
        {
            const auto& rows=*slot->cohort_rows[index];
            id<MTLBuffer> native=(__bridge id<MTLBuffer>)rows.buffer->nativeHandle();
            const auto address=native.gpuAddress;
            if(!address || native.device!=owner.native_device)
                return reject("Retained visibility row GPU address is unavailable",ALMetalWorldVisibilityFailure::cohort_batch_unsupported);
            auto& descriptor=descriptors[index];descriptor.rows_address=address;
            std::memcpy(descriptor.matrix,cohorts[index].view.modelview_projection,sizeof(descriptor.matrix));
            descriptor.config[0]=static_cast<std::uint32_t>(rows.count);
            descriptor.config[1]=static_cast<std::uint32_t>(cohorts[index].logical_begin);
            descriptor.config[2]=static_cast<std::uint32_t>(packed_count);packed_count+=rows.count;
            indirect_rows.push_back((__bridge void*)native);
        }
        const auto bytes=descriptors.size()*sizeof(CohortDispatch);
        if(!slot->cohort_descriptors || slot->cohort_descriptors->buffer->length()<bytes)
        {
            std::size_t capacity=4096;while(capacity<bytes)capacity*=2;
            if(capacity>maximum_cohort_descriptor_bytes)
                return reject("Packed visibility descriptor capacity is exhausted",ALMetalWorldVisibilityFailure::cohort_descriptors);
            // Only completed, unborrowed slots may release their arena debt.
            // The selected slot is also quiescent; its extra local owner is
            // not a reader. Pending/borrowed descriptor generations survive.
            slot->cohort_descriptors.reset();
            auto retained=owner.live_descriptor_bytes->load(std::memory_order_relaxed);
            if(retained>maximum_cohort_descriptor_bytes-capacity)
                for(const auto& candidate:owner.slots)
                    if(candidate!=slot && candidate.use_count()==1 &&
                       (!candidate->consumer || candidate->consumer->isComplete()))
                        candidate->cohort_descriptors.reset();
            retained=owner.live_descriptor_bytes->load(std::memory_order_relaxed);
            if(retained>maximum_cohort_descriptor_bytes-capacity)
                return reject("Packed visibility physical descriptor capacity is exhausted",ALMetalWorldVisibilityFailure::cohort_descriptors);
            auto allocation=std::make_unique<CohortDescriptors>();
            ALMetalBufferDescriptor descriptor;descriptor.length=capacity;descriptor.storage=ALMetalStorageMode::shared;
            descriptor.label="Alchemy packed cohort visibility descriptors";
            allocation->buffer=ALMetalBuffer::create(*owner.device,descriptor,error);
            if(!allocation->buffer)return reject("Could not allocate packed visibility descriptors",ALMetalWorldVisibilityFailure::cohort_descriptors);
            const auto allocated=[(__bridge id<MTLBuffer>)allocation->buffer->nativeHandle() allocatedSize];
            if(!allocated || allocated<capacity || allocated>maximum_cohort_descriptor_bytes-retained)
                return reject("Packed visibility physical descriptor capacity is exhausted",ALMetalWorldVisibilityFailure::cohort_descriptors);
            allocation->live=owner.live_descriptor_bytes;allocation->bytes=allocated;
            allocation->live->fetch_add(allocated,std::memory_order_relaxed);
            owner.statistics.cohort_descriptor_allocated_bytes+=allocated;slot->cohort_descriptors=std::move(allocation);
        }
        if(!slot->cohort_descriptors->buffer->write(0,descriptors.data(),bytes,error))
            return reject("Could not publish packed visibility descriptors",ALMetalWorldVisibilityFailure::publication);
        owner.statistics.cohort_descriptor_upload_bytes+=bytes;
    }
    slot->bounds=raw_bounds?owner.current:nullptr;slot->consumer=completion;slot->generation=slot->bounds?slot->bounds->serial:0;
    slot->native_command=(__bridge id<MTLCommandBuffer>)command.nativeHandle();
    auto result=std::shared_ptr<ALMetalWorldVisibilityArguments>(new ALMetalWorldVisibilityArguments(slot,view.mImpl));
    if(slot->bounds)slot->bounds->readers.push_back(completion);
    if(!command.retainUntilCompletion(slot,error)){fallback(owner.statistics,ALMetalWorldVisibilityFailure::retention);return {};}
    auto encoder=[slot->native_command computeCommandEncoder];
    if(!encoder)return reject("Could not begin shared visibility compute",ALMetalWorldVisibilityFailure::encoder);
    if(!members.empty())
    {
        [encoder setComputePipelineState:owner.pipeline];
        const auto* bounds=slot->bounds?slot->bounds->buffer.get():slot->members.get();
        [encoder setBuffer:(__bridge id<MTLBuffer>)bounds->nativeHandle() offset:0 atIndex:0];
        [encoder setBuffer:(__bridge id<MTLBuffer>)slot->members->nativeHandle() offset:0 atIndex:1];
        [encoder setBuffer:(__bridge id<MTLBuffer>)slot->arguments->nativeHandle() offset:0 atIndex:2];
        const std::array<std::uint32_t,4> counts{static_cast<std::uint32_t>(members.size()),
            static_cast<std::uint32_t>(owner.records.size()),mapped_destinations,0};
        [encoder setBytes:counts.data() length:sizeof(counts) atIndex:3];
        const auto* destinations=mapped_destinations?slot->destinations.get():slot->members.get();
        [encoder setBuffer:(__bridge id<MTLBuffer>)destinations->nativeHandle() offset:0 atIndex:4];
        [encoder dispatchThreads:MTLSizeMake(members.size(),1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
    }
    if(batched)
    {
        [encoder setComputePipelineState:owner.cohort_batch_pipeline];
        [encoder setBuffer:(__bridge id<MTLBuffer>)slot->cohort_descriptors->buffer->nativeHandle() offset:0 atIndex:0];
        [encoder setBuffer:(__bridge id<MTLBuffer>)slot->arguments->nativeHandle() offset:0 atIndex:1];
        const std::array<std::uint32_t,4> counts{static_cast<std::uint32_t>(packed_count),static_cast<std::uint32_t>(cohorts.size()),0,0};
        [encoder setBytes:counts.data() length:sizeof(counts) atIndex:2];
        // A GPU address neither retains its allocation nor declares residency.
        // The command-owned slot supplies lifetime; declare the complete union.
        const auto* resources=reinterpret_cast<id<MTLResource> __unsafe_unretained const*>(indirect_rows.data());
        [encoder useResources:resources count:indirect_rows.size() usage:MTLResourceUsageRead];
        [encoder dispatchThreads:MTLSizeMake(packed_count,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
        ++owner.statistics.cohort_dispatches;++owner.statistics.batched_cohort_dispatches;
    }
    else if(!cohorts.empty())
    {
        constexpr std::size_t index=0;
        struct alignas(16) CohortViewData{float matrix[16];std::uint32_t config[4];};
        CohortViewData data{};std::memcpy(data.matrix,cohorts[index].view.modelview_projection,sizeof(data.matrix));
        data.config[0]=static_cast<std::uint32_t>(slot->cohort_rows[index]->count);
        data.config[1]=static_cast<std::uint32_t>(cohorts[index].logical_begin);
        [encoder setComputePipelineState:owner.cohort_pipeline];
        [encoder setBuffer:(__bridge id<MTLBuffer>)slot->cohort_rows[index]->buffer->nativeHandle() offset:0 atIndex:0];
        [encoder setBytes:&data length:sizeof(data) atIndex:1];
        [encoder setBuffer:(__bridge id<MTLBuffer>)slot->arguments->nativeHandle() offset:0 atIndex:2];
        [encoder dispatchThreads:MTLSizeMake(slot->cohort_rows[index]->count,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
        ++owner.statistics.cohort_dispatches;
    }
    [encoder endEncoding];++owner.statistics.views;owner.statistics.argument_candidates+=candidates+cohort_candidates;
    owner.statistics.expanded_member_bytes+=members.size()*sizeof(ALMetalWorldVisibilityMember);
    if(!cohorts.empty()){++owner.statistics.cohort_views;owner.statistics.cohort_groups+=cohorts.size();
        owner.statistics.cohort_candidates+=cohort_candidates;}
    return result;
}
catch(const std::bad_alloc&)
{
    fallback(view.mImpl->owner->statistics,ALMetalWorldVisibilityFailure::host_allocation);
    if(error)error->clear();return {};
}
ALMetalWorldVisibilityStats ALMetalWorldVisibility::stats() const noexcept
{
    auto value=mImpl->statistics;value.live_cohort_row_bytes=mImpl->live_cohort_bytes->load(std::memory_order_relaxed);
    value.live_cohort_descriptor_bytes=mImpl->live_descriptor_bytes->load(std::memory_order_relaxed);
    value.cohort_descriptor_limit_bytes=maximum_cohort_descriptor_bytes;
    for(const auto& [owner,entry]:mImpl->cohort_cache)value.cached_cohort_rows+=entry.rows->count;
    return value;
}
