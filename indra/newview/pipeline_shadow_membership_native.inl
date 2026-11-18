/**
 * @brief Owner-side admitted-group bridge; no live scene pointers in packets.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include after pipeline_scene_publication_native.inl on the scene owner.
 */
#include "../almetal/almetalshadowpublication.h"

namespace ALSceneShadowMembershipNative
{
inline constexpr std::size_t minimum_members = 4;
// The measured tiny/changing cases retain their existing raw path. This is
// solely optional preparation profitability; immutable cache validity still
// uses the exact group/source revisions and dependencies below.
inline bool candidateReady(ALNativeCohortCompilationAdmission& stability,
    const std::shared_ptr<ALNativePublication::State>& owner,
    ALNativeFrameBudget::Frame frame, std::size_t count) noexcept
{
    return count >= minimum_members && owner &&
        !stability.deferUnstableRevision(frame,
            ALNativeCohortCompilationAdmission::ProducerStamp{owner->identity(),owner->revision()});
}

struct AdmittedSpan
{
    std::weak_ptr<ALNativePublication::State> owner;
    std::uint64_t identity = 0, revision = 0;
    std::uint32_t pass = 0;
    ALMetalWorldSpotShadowCoverage coverage = ALMetalWorldSpotShadowCoverage::opaque;
    std::size_t first = 0, count = 0;
    // True only for the whole original group's pass, after the established
    // positive-count filter. A per-draw subset must keep the raw owner path.
    bool whole_group = false;
};
inline std::uint64_t flavor(const AdmittedSpan& span) noexcept
{
    return (std::uint64_t(span.pass) << 32u) | std::uint32_t(span.coverage);
}
inline AdmittedSpan admit(const std::shared_ptr<ALNativePublication::State>& owner,
    std::uint32_t pass, ALMetalWorldSpotShadowCoverage coverage,
    std::size_t first, std::size_t count, bool whole_group) noexcept
{
    return {owner,owner ? owner->identity() : 0,owner ? owner->revision() : 0,
        pass,coverage,first,count,whole_group};
}
inline std::shared_ptr<ALNativePublication::State> currentOwner(const AdmittedSpan& span) noexcept
{
    auto owner=span.owner.lock();
    return span.whole_group && owner && owner->identity()==span.identity && owner->revision()==span.revision
        ? owner : nullptr;
}
inline ALMetalShadowMembershipPublisher::Result current(ALMetalShadowMembershipCache& cache,
    const AdmittedSpan& span, ALNativeSourceWorkingSet& residency, std::uint32_t frame,
    std::optional<ALNativeFrameBudget::Frame> preparation_frame = std::nullopt)
{
    const auto owner=currentOwner(span);
    if(!owner)return {{},{},ALMetalShadowMembershipPublisher::Failure::changed_source};
    auto result=cache.current(owner,flavor(span),residency,frame,preparation_frame);
    if(result && (result.membership->members().size()!=span.count || result.membership->ranges().size()!=1 ||
        result.membership->ranges()[0]!=ALMetalShadowMembershipRange{span.pass,span.coverage,0,span.count}))
        return {{},{},ALMetalShadowMembershipPublisher::Failure::invalid_ranges};
    return result;
}
inline ALMetalShadowMembershipPublisher::Result seal(ALMetalShadowMembershipCache& cache,
    const AdmittedSpan& span, std::span<const ALMetalShadowMembershipSource> inputs,
    ALNativeSourceWorkingSet& residency, ALNativeFrameBudget& budget,
    std::uint64_t& serial, std::uint32_t frame, ALNativeFrameBudget::Frame oldest)
{
    const auto owner=currentOwner(span);
    if(!owner)return {{},{},ALMetalShadowMembershipPublisher::Failure::changed_source};
    if(inputs.size()!=span.count)return {{},{},ALMetalShadowMembershipPublisher::Failure::invalid_ranges};
    const ALMetalShadowMembershipRange range{span.pass,span.coverage,0,span.count};
    auto result=cache.seal(owner,flavor(span),inputs,std::span(&range,1),residency,budget,serial,frame,oldest);
    if(owner->revision()!=span.revision)return {{},{},ALMetalShadowMembershipPublisher::Failure::changed_source};
    return result;
}
}
