/**
 * @file pipeline_cohort_compilation_native.inl
 * @brief Owner-only optional cohort compilation credit and work estimate.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include after pipeline_group_publication_native.inl.
 */
#include "alnativeframebudget.h"
namespace ALSceneGroupCompilationNative
{
struct Statistics
{
    U64 candidates=0,duplicate_views=0,admitted=0,denied=0,published=0,failed=0;
    U64 requested_bytes=0,admitted_bytes=0,saturated=0;
    std::array<U64,6> reasons{};
};
inline Statistics statistics;

// Conservative logical work for the selected source/proof/owner descriptors
// and their worst-case slot/union tuples. These are not resident asset bytes,
// allocator bookkeeping, a CPU timer, or a latency guarantee. The actual CPU
// cache and native working set retain their independent measured byte caps.
inline U64 workBytes(std::size_t rows, U32 family) noexcept
{
    constexpr U64 base=sizeof(ALSceneGroupPublicationNative::Payload)+sizeof(ALSceneGroupPublicationNative::Layout);
    constexpr U64 row=sizeof(ALSceneGroupPublicationNative::Source)+2*sizeof(ALMetalWorldSimpleBatch)+
        sizeof(ALSceneGroupPublicationNative::SourceLease)+sizeof(ALMetalWorldKeyedSource)+
        sizeof(std::shared_ptr<const ALMetalWorldCohortSource>);
    constexpr U64 tuple=2*sizeof(ALSceneGroupPublicationNative::WeakResource)+
        2*sizeof(ALNativeSourceWorkingSet::Resource)+sizeof(std::weak_ptr<const ALMetalTexture>);
    const U64 roles=family==0?ALMetalWorldSimpleBatch::max_diffuse_textures+2:
        ALMetalWorldSimpleBatch::max_material_slots*4+5;
    constexpr U64 unknown=ALNativeFrameBudget::unknown_bytes;
    if(roles>(unknown-row)/tuple)return unknown;
    const U64 per_row=row+roles*tuple;
    return rows>(unknown-1-base)/per_row?unknown:base+U64(rows)*per_row;
}

template<class Compile>
std::shared_ptr<const ALSceneGroupPublicationNative::Payload> compile(
    ALNativeCohortCompilationAdmission& admission,
    const std::shared_ptr<ALNativePublication::State>& publisher,
    ALNativeFrameBudget& budget,std::size_t rows,U32 family,Compile&& produce)
{
    const auto frame=budget.snapshot().frame;
    admission.notePending(frame);
    if(admission.lastConsideredFrame() && frame<=*admission.lastConsideredFrame())
    {++statistics.duplicate_views;return {};}
    const U64 bytes=workBytes(rows,family);
    if(bytes==ALNativeFrameBudget::unknown_bytes)++statistics.saturated;
    else statistics.requested_bytes+=bytes;
    auto decision=admission.tryAcquire(budget,bytes);
    ++statistics.reasons[static_cast<std::size_t>(decision.reason)];
    if(!decision){++statistics.denied;return {};}
    ++statistics.admitted;statistics.admitted_bytes+=bytes;
    // CPU-local credit covers only this synchronous compile + publication.
    // Required raw capture still runs when denied, and GPU metadata has its
    // own distinct later admission on the same application frame budget.
    try
    {
        auto result=produce();
        if(result && result->cohort && !publisher->dirty() && publisher->publication().get()==result.get())
        {admission.publicationCompleted();++statistics.published;return result;}
    }
    catch(const std::bad_alloc&){}
    ++statistics.failed;return {};
}
}
