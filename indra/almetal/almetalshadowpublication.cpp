/** SPDX-License-Identifier: LGPL-2.1-or-later */
#include "almetalshadowpublication.h"
#include "almetalresource.h"

#include <algorithm>
#include <limits>
#include <map>
#include <new>
#include <optional>

struct ALMetalShadowMembershipReservation
{
    std::shared_ptr<ALMetalShadowMembershipAccounting> accounting;
    std::uint64_t bytes = 0;
    bool acquired = false;
    ALMetalShadowMembershipReservation(std::shared_ptr<ALMetalShadowMembershipAccounting> domain,
                                      std::uint64_t count) noexcept : accounting(std::move(domain)), bytes(count)
    {
        auto old = accounting->mBytes.load(std::memory_order_relaxed);
        while (old <= accounting->mMaximum && bytes <= accounting->mMaximum - old)
            if (accounting->mBytes.compare_exchange_weak(old, old + bytes,
                    std::memory_order_relaxed, std::memory_order_relaxed))
            { acquired = true; break; }
    }
    ~ALMetalShadowMembershipReservation()
    { if (acquired) accounting->mBytes.fetch_sub(bytes,std::memory_order_relaxed); }
    void reduceTo(std::uint64_t smaller) noexcept
    {
        if (acquired && smaller <= bytes)
        { accounting->mBytes.fetch_sub(bytes-smaller,std::memory_order_relaxed);bytes=smaller; }
    }
    struct Transferred {};
    ALMetalShadowMembershipReservation(std::shared_ptr<ALMetalShadowMembershipAccounting> domain,
        std::uint64_t count, Transferred) noexcept : accounting(std::move(domain)),bytes(count),acquired(true) {}
    std::shared_ptr<ALMetalShadowMembershipReservation> transfer(std::uint64_t count)
    {
        if(!acquired || count>bytes)return {};
        auto child=std::make_shared<ALMetalShadowMembershipReservation>(accounting,count,Transferred{});
        bytes-=count; // Transfer existing debt; never manufacture/reacquire credit.
        return child;
    }
};

struct ALMetalShadowMembership::NativeResources
{
    // Declared first, so its own vector/control-block capacity debt retires
    // AFTER the resource vector and its native ownership leases are destroyed.
    std::shared_ptr<const ALMetalShadowMembershipReservation> reservation;
    std::vector<ALNativeSourceWorkingSet::Resource> values;
};
std::span<const ALNativeSourceWorkingSet::Resource> ALMetalShadowMembership::resources() const noexcept
{ return mResources ? std::span<const ALNativeSourceWorkingSet::Resource>(mResources->values) : std::span<const ALNativeSourceWorkingSet::Resource>{}; }

namespace
{
using Resource = ALNativeSourceWorkingSet::Resource;
using Failure = ALMetalShadowMembershipPublisher::Failure;
using Source = ALMetalShadowMembershipSource;

template<class A, class B>
bool sameOwner(const std::shared_ptr<A>& a,
               const std::shared_ptr<B>& b) noexcept
{
    return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
}
bool current(const Source& input) noexcept
{
    return input.producer && input.record && input.identity && input.revision &&
        input.record->generation() && input.producer->identity() == input.identity &&
        input.producer->revision() == input.revision;
}
bool current(std::span<const Source> inputs) noexcept
{
    return std::all_of(inputs.begin(), inputs.end(), [](const auto& row) { return current(row); });
}
bool add(std::uint64_t& destination, std::uint64_t value) noexcept
{
    if (value > std::numeric_limits<std::uint64_t>::max() - destination) return false;
    destination += value; return true;
}
bool product(std::uint64_t a, std::uint64_t b, std::uint64_t& result) noexcept
{
    if (a && b > std::numeric_limits<std::uint64_t>::max() / a) return false;
    result = a * b; return true;
}
bool textureBytes(const ALMetalTexture& texture, std::uint64_t& bytes) noexcept
{
    const auto& d = texture.descriptor();
    std::uint64_t slices = d.array_length;
    if (d.type == ALMetalTextureType::texture_cube) slices = 6;
    else if (d.type == ALMetalTextureType::texture_cube_array && !product(slices, 6, slices)) return false;
    bytes = 0;
    for (std::uint32_t mip = 0; mip < d.mipmap_level_count; ++mip)
    {
        const auto bw = std::max<std::uint64_t>(texture.blockWidth(), 1);
        const auto bh = std::max<std::uint64_t>(texture.blockHeight(), 1);
        std::uint64_t level = (std::uint64_t(texture.width(mip)) + bw - 1) / bw;
        if (!product(level, (std::uint64_t(texture.height(mip)) + bh - 1) / bh, level) ||
            !product(level, texture.bytesPerBlock(), level) ||
            !product(level, std::max<std::uint64_t>(slices, 1), level) ||
            !product(level, std::max<std::uint64_t>(d.sample_count, 1), level) ||
            !add(bytes, level)) return false;
    }
    return bytes != 0;
}
bool coverageMatches(ALMetalWorldSpotShadowCoverage policy,
                     ALMetalWorldSpotShadowCoverage actual) noexcept
{
    // Existing PASS_ALPHA source factory maps GLTF legacy_blend to pbr_blend.
    return policy == actual || (policy == ALMetalWorldSpotShadowCoverage::legacy_blend &&
        actual == ALMetalWorldSpotShadowCoverage::pbr_blend);
}
bool rangesValid(std::span<const Source> inputs,
                 std::span<const ALMetalShadowMembershipRange> ranges) noexcept
{
    std::size_t cursor = 0;
    for (const auto& range : ranges)
    {
        if (range.first != cursor || range.count > inputs.size() - cursor ||
            range.coverage > ALMetalWorldSpotShadowCoverage::pbr_blend) return false;
        for (std::size_t i = 0; i < range.count; ++i)
            if (!inputs[cursor + i].record ||
                !coverageMatches(range.coverage, inputs[cursor + i].record->prototype().coverage)) return false;
        cursor += range.count;
    }
    return cursor == inputs.size();
}
bool matches(const ALMetalShadowMembership& publication, std::span<const Source> inputs,
             std::span<const ALMetalShadowMembershipRange> ranges) noexcept
{
    if (publication.members().size() != inputs.size() ||
        publication.ranges().size() != ranges.size() ||
        !std::equal(ranges.begin(), ranges.end(), publication.ranges().begin())) return false;
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        const auto& old = publication.members()[i]; const auto& next = inputs[i];
        if (old.identity != next.identity || old.revision != next.revision ||
            !sameOwner(old.record, next.record)) return false;
    }
    return true;
}
}

struct ALMetalShadowMembershipPublisher::SourceWatch
{
    // Last-reader debt includes this witness, its child observer and the
    // conservative logical subscription capacity. It is independent of both
    // the large CPU rows and native-only completion owners.
    std::shared_ptr<const ALMetalShadowMembershipReservation> reservation;
    ALNativePublication::State state;
};

class ALMetalShadowMembershipPublisher::Dependencies final : public ALNativePublication::Observer
{
public:
    explicit Dependencies(const std::shared_ptr<ALNativePublication::State>& state,
                          std::uint64_t expected_revision=0,
                          std::shared_ptr<SourceWatch> witness={})
        : mWitness(std::move(witness)),mState(state),mExpectedRevision(expected_revision) {}
    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        // Individual cache eviction is not backing mutation; this publication
        // has its own ledger credit and immutable readers. Real writes still
        // synchronously drop it BEFORE the source tests native backing COW.
        if (phase != ALNativePublication::Phase::withdrawn)
        {
            // Only this immutable source set's own counter changes. An
            // obsolete callback cannot affect a newer installed witness.
            // Keep observing through repeated writes after payload withdrawal.
            if(mWitness)mWitness->state.invalidate();
            if (auto state = mState.lock();state &&
                (!mExpectedRevision || state->revision()==mExpectedRevision)) state->invalidate();
        }
    }
private:
    // Retire the witness/debt after the remaining observer host fields.
    std::shared_ptr<SourceWatch> mWitness;
    std::weak_ptr<ALNativePublication::State> mState;
    const std::uint64_t mExpectedRevision;
};

ALMetalShadowMembershipPublisher::ALMetalShadowMembershipPublisher()
    : ALMetalShadowMembershipPublisher(Limits{}) {}
ALMetalShadowMembershipPublisher::ALMetalShadowMembershipPublisher(Limits limits)
    : mState(std::make_shared<ALNativePublication::State>()),
      mOwnerDependencies(std::make_shared<Dependencies>(mState)),
      mAccounting(std::make_shared<ALMetalShadowMembershipAccounting>(limits.maximum_metadata_bytes)), mLimits(limits) {}
ALMetalShadowMembershipPublisher::~ALMetalShadowMembershipPublisher() = default;
void ALMetalShadowMembershipPublisher::invalidate() noexcept
{ mState->invalidate();mChildDependencies.reset();mSourceWitness.reset(); }

ALMetalShadowMembershipPublisher::Result ALMetalShadowMembershipPublisher::seal(
    std::span<const Source> inputs, std::span<const ALMetalShadowMembershipRange> ranges,
    ALNativeSourceWorkingSet& working_set, ALNativeFrameBudget& budget,
    std::uint64_t& serial, std::uint32_t frame, ALNativeFrameBudget::Frame oldest_dirty_frame)
{
    std::shared_ptr<const void> attempted_publication;
    const auto decline = [&](Failure failure) {
        ++mStats.declined; return Result{{}, Outcome::unavailable, failure};
    };
    try
    {
        if (inputs.empty() || !current(inputs)) return decline(Failure::invalid_source);
        if (inputs.size() > mLimits.maximum_members || ranges.size() > mLimits.maximum_ranges)
            return decline(Failure::bounds);
        if (!rangesValid(inputs, ranges)) return decline(Failure::invalid_ranges);
        if (auto existing = mState->publication())
        {
            auto old = std::static_pointer_cast<const ALMetalShadowMembership>(existing);
            if (matches(*old, inputs, ranges) &&
                working_set.touch(mState->identity(), old->serial(), frame) && current(inputs))
            {
                ++mStats.reused; return {std::move(old), Outcome::reused, Failure::none};
            }
        }
        // Every miss receives a distinct derived revision, including a miss
        // after cache withdrawal (which itself deliberately preserves source
        // revisions). An older in-progress child observer can never invalidate
        // a nested newer publication even while the old callback stays alive.
        // A denied attempt must not discard the last successful source-set
        // witness. Its callbacks still report later writes, while their old
        // expected revision cannot invalidate this newer attempt's payload.
        mState->invalidate();
        const auto expected = mState->revision();
        // Bound peak copied metadata before allocating. Duplicate resources
        // may make the actual union smaller; do not undercharge dirty work.
        std::uint64_t logical = sizeof(ALMetalShadowMembership), rows = 0, resource_count = 0;
        if (!product(inputs.size(), sizeof(ALMetalShadowMembership::Member) + sizeof(Source), rows) ||
            !add(logical, rows) || !product(ranges.size(), sizeof(ALMetalShadowMembershipRange), rows) ||
            !add(logical, rows)) return decline(Failure::bounds);
        for (const auto& input : inputs)
            if (!add(resource_count, 2u + input.record->textureLeases().size())) return decline(Failure::bounds);
        std::uint64_t watch_bytes=sizeof(SourceWatch)+sizeof(Dependencies)+
            sizeof(ALMetalShadowMembershipReservation)+3u*4u*sizeof(void*);
        // Each distinct source may need its first Signal plus grown weak-link
        // storage. Duplicate input sources conservatively overcharge. This is
        // logical subscription capacity, not a physical allocator/UMA claim.
        if(!product(inputs.size(),sizeof(ALNativePublication::Signal)+4u*sizeof(void*)+
                2u*sizeof(std::weak_ptr<ALNativePublication::Observer>),rows)||
            !add(watch_bytes,rows)||!add(logical,watch_bytes))return decline(Failure::bounds);
        if (!product(resource_count, sizeof(Resource), rows) || !add(logical, rows) ||
            logical > mLimits.maximum_metadata_bytes) return decline(Failure::bounds);
        auto admission = budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,
            logical, oldest_dirty_frame);
        if (!admission) return decline(Failure::admission);
        // Reserve a conservative dirty-build envelope BEFORE constructing
        // another CPU payload while older unjoined readers still own debt.
        // Duplicate records deliberately overreserve until the exact unique
        // retained tuple is known. This may decline optional caching earlier;
        // it never removes required serial rendering or invents free credit.
        std::uint64_t reservation_bytes=logical;
        if (!add(reservation_bytes,sizeof(ALMetalShadowMembership::NativeResources)+
                2u*sizeof(ALMetalShadowMembershipReservation)+4u*4u*sizeof(void*))) return decline(Failure::bounds);
        for(const auto& input:inputs)
            if(!add(reservation_bytes,input.record->ownedBytes()))return decline(Failure::bounds);
        auto reservation=std::make_shared<ALMetalShadowMembershipReservation>(mAccounting,reservation_bytes);
        if(!reservation->acquired)return decline(Failure::cpu_residency);
        struct CompactAfterScratch
        {
            std::shared_ptr<ALMetalShadowMembershipReservation> reservation;
            std::uint64_t retained_bytes=0;
            bool successful=false;
            ~CompactAfterScratch(){if(successful)reservation->reduceTo(retained_bytes);}
        } compact{reservation};
        // Own source States during notification/retain callbacks only. They
        // never enter the immutable payload handed to worker/native owners.
        const std::vector<Source> snapshot(inputs.begin(), inputs.end());
        if (!current(snapshot) || mState->revision() != expected) return decline(Failure::changed_source);
        auto witness=std::make_shared<SourceWatch>();
        witness->reservation=reservation->transfer(watch_bytes);
        if(!witness->reservation)return decline(Failure::bounds);
        auto child_dependencies=std::make_shared<Dependencies>(mState,expected,witness);
        auto value = std::make_shared<ALMetalShadowMembership>();
        value->mCPUReservation=reservation;
        value->mMembers.reserve(snapshot.size());
        value->mRanges.assign(ranges.begin(), ranges.end());
        // Native ownership/control blocks, not pointer addresses, define the
        // exact union. Descriptor walks happen only on this dirty path.
        std::map<std::shared_ptr<const void>, std::uint64_t,
            std::owner_less<std::shared_ptr<const void>>> resources;
        const auto append = [&](std::shared_ptr<const void> allocation, std::uint64_t bytes) {
            if (!allocation || !bytes) return false;
            const auto [at, inserted] = resources.emplace(std::move(allocation), bytes);
            return inserted || at->second == bytes;
        };
        for (const auto& input : snapshot)
        {
            input.producer->observe(child_dependencies);
            const auto& source = input.record;
            if (!source->vertexLease() || !source->indexLease() ||
                !append(source->vertexLease(), source->vertexLease()->length()) ||
                !append(source->indexLease(), source->indexLease()->length()))
                return decline(Failure::resource_union);
            for (const auto& texture : source->textureLeases())
            {
                std::uint64_t bytes = 0;
                if (!texture || !textureBytes(*texture, bytes) || !append(texture, bytes))
                    return decline(Failure::resource_union);
            }
            value->mMembers.push_back({input.producer, input.identity, input.revision, source});
        }
        auto native_resources = std::make_shared<ALMetalShadowMembership::NativeResources>();
        native_resources->values.reserve(resources.size());
        for (const auto& [allocation, bytes] : resources) native_resources->values.push_back({allocation, bytes});
        value->mNativeMetadataBytes=sizeof(ALMetalShadowMembership::NativeResources)+
            sizeof(ALMetalShadowMembershipReservation)+2u*4u*sizeof(void*);
        if(!product(native_resources->values.capacity(),sizeof(Resource),rows)||
            !add(value->mNativeMetadataBytes,rows))return decline(Failure::bounds);
        native_resources->reservation=reservation->transfer(value->mNativeMetadataBytes);
        if(!native_resources->reservation)return decline(Failure::bounds);
        value->mResources = std::move(native_resources);
        value->mMetadataBytes = sizeof(ALMetalShadowMembership) +
            sizeof(ALMetalShadowMembershipReservation)+2u*4u*sizeof(void*);
        if (!product(value->mMembers.capacity(), sizeof(ALMetalShadowMembership::Member), rows) ||
            !add(value->mMetadataBytes, rows) ||
            !product(value->mRanges.capacity(), sizeof(ALMetalShadowMembershipRange), rows) ||
            !add(value->mMetadataBytes, rows) || !add(value->mMetadataBytes,value->mNativeMetadataBytes) ||
            value->mMetadataBytes > mLimits.maximum_metadata_bytes)
            return decline(Failure::bounds);
        // A membership may be the last CPU owner after its individual source
        // cache is withdrawn. Charge each retained exact Record once, including
        // its texture-vector CAPACITY and immutable coverage allocation.
        std::map<std::shared_ptr<const ALMetalShadowSourceRecord>,bool,
            std::owner_less<std::shared_ptr<const ALMetalShadowSourceRecord>>> counted_records;
        for (const auto& member : value->mMembers)
            if (counted_records.emplace(member.record,true).second &&
                (!add(value->mMetadataBytes, member.record->ownedBytes()) ||
                 value->mMetadataBytes > mLimits.maximum_metadata_bytes)) return decline(Failure::bounds);
        if (!current(snapshot) || mState->revision() != expected) return decline(Failure::changed_source);
        if(value->mMetadataBytes>reservation_bytes ||
            watch_bytes>mLimits.maximum_metadata_bytes-value->mMetadataBytes)return decline(Failure::bounds);
        if (serial == std::numeric_limits<std::uint64_t>::max()) return decline(Failure::publication);
        value->mSerial = ++serial; value->mRevision = expected;
        if (!mState->publish(expected, value)) return decline(Failure::changed_source);
        const std::shared_ptr<const void> publication = value;
        attempted_publication = publication;
        const bool retained = working_set.retain(mState, value->mSerial, publication, value->resources(), frame);
        if (!current(snapshot) || mState->revision() != expected) return decline(Failure::changed_source);
        // Publish only this exact generation's dependency lease. A stale outer
        // attempt must not replace an observer installed by a nested newer seal.
        if(sameOwner(mState->publication(),publication)||!mState->publication())
        {mChildDependencies=std::move(child_dependencies);mSourceWitness=std::move(witness);}
        ++mStats.sealed; ++mStats.resource_unions;
        if (!retained) ++mStats.transient;
        compact.retained_bytes=value->mMetadataBytes-value->mNativeMetadataBytes;compact.successful=true;
        // Local CPU publication credit ends after the synchronous seal. Native
        // residency and actual command readers have independent existing owners.
        return {std::move(value), retained ? Outcome::retained : Outcome::transient, Failure::none, true};
    }
    catch (const std::bad_alloc&)
    {
        if (attempted_publication && sameOwner(mState->publication(), attempted_publication))
            mState->evict();
        return decline(Failure::allocation);
    }
}

struct ALMetalShadowMembershipCache::Impl
{
    using Key = std::pair<std::uint64_t,std::uint64_t>;
    struct Entry
    {
        std::shared_ptr<const ALMetalShadowMembershipReservation> reservation;
        std::weak_ptr<ALNativePublication::State> owner;
        std::uint64_t owner_revision = 0, last_used = 0;
        std::shared_ptr<ALMetalShadowMembershipPublisher> publisher;
        std::optional<ALNativeFrameBudget::Frame> first_unserved;
        ALNativeCohortCompilationAdmission source_stability;
        std::optional<ALNativeFrameBudget::Frame> last_prepare_attempt;
    };
    Limits limits;
    std::map<Key,Entry> entries;
    Stats counts;
    std::uint64_t clock = 0;
    std::shared_ptr<ALMetalShadowMembershipAccounting> accounting;
    // Logical conservative charge for the entry/key/tree node and its small
    // State/observer owner objects, separately from copied membership storage.
    static constexpr std::uint64_t entry_charge = sizeof(Entry)+sizeof(Key)+
        sizeof(ALMetalShadowMembershipPublisher)+sizeof(ALNativePublication::State)+
        sizeof(ALMetalShadowMembershipPublisher::Dependencies)+
        sizeof(ALMetalShadowMembershipReservation)+sizeof(ALMetalShadowMembershipAccounting)+
        // Five conservative shared-control blocks, plus red-black tree links,
        // colour/alignment storage. Exact vector/Record capacities are charged
        // by the payload reservations, not hidden inside this entry estimate.
        (5u*4u+4u)*sizeof(void*);
    std::uint64_t bytes() const noexcept
    {
        return accounting->bytes();
    }
    void erase(std::map<Key,Entry>::iterator at) noexcept
    {
        at->second.publisher->invalidate();
        entries.erase(at); ++counts.evictions;
    }
    bool trim(const Key* protected_key = nullptr) noexcept
    {
        while (entries.size() > limits.maximum_entries || bytes() > limits.maximum_metadata_bytes)
        {
            auto victim = entries.end();
            for (auto at = entries.begin(); at != entries.end(); ++at)
                if ((!protected_key || at->first != *protected_key) &&
                    (victim == entries.end() || at->second.last_used < victim->second.last_used)) victim = at;
            if (victim == entries.end()) return false;
            erase(victim);
        }
        return true;
    }
};

ALMetalShadowMembershipCache::ALMetalShadowMembershipCache() : ALMetalShadowMembershipCache(Limits{}) {}
ALMetalShadowMembershipCache::ALMetalShadowMembershipCache(Limits limits)
    : ALMetalShadowMembershipCache(limits,std::make_shared<ALMetalShadowMembershipAccounting>(limits.maximum_metadata_bytes)) {}
ALMetalShadowMembershipCache::ALMetalShadowMembershipCache(Limits limits,
    std::shared_ptr<ALMetalShadowMembershipAccounting> accounting) : mImpl(std::make_unique<Impl>())
{
    mImpl->limits = limits;
    mImpl->accounting = accounting ? std::move(accounting)
        : std::make_shared<ALMetalShadowMembershipAccounting>(limits.maximum_metadata_bytes);
}
ALMetalShadowMembershipCache::~ALMetalShadowMembershipCache() = default;

ALMetalShadowMembershipCache::Result ALMetalShadowMembershipCache::current(
    const std::shared_ptr<ALNativePublication::State>& owner, std::uint64_t flavor,
    ALNativeSourceWorkingSet& working_set, std::uint32_t frame,
    std::optional<ALNativeFrameBudget::Frame> preparation_frame)
{
    if (!owner) return {{}, {}, Failure::invalid_source};
    const Impl::Key key{owner->identity(),flavor};
    auto at = mImpl->entries.find(key);
    if(at==mImpl->entries.end()&&preparation_frame)
    {
        const auto maximum=std::min(mImpl->limits.maximum_metadata_bytes,mImpl->accounting->maximumBytes());
        // Only a guaranteed hard impossibility. Existing local idle entries
        // may retire debt during seal, so do not predict general pressure.
        if(!mImpl->limits.maximum_entries||Impl::entry_charge>maximum||
            (mImpl->entries.empty()&&mImpl->bytes()>maximum-Impl::entry_charge))
        {++mImpl->counts.misses;Result result;result.defer_preparation=true;return result;}
    }
    if (at != mImpl->entries.end())
    {
        auto publisher = at->second.publisher;
        if (sameOwner(at->second.owner.lock(), owner) && at->second.owner_revision == owner->revision())
        {
            if (auto native = publisher->mState->publication())
            {
                auto value = std::static_pointer_cast<const ALMetalShadowMembership>(native);
                if (working_set.touch(publisher->mState->identity(), value->serial(), frame))
                {
                    at->second.last_used = ++mImpl->clock; ++mImpl->counts.hits;
                    at->second.first_unserved.reset();
                    return {std::move(value), ALMetalShadowMembershipPublisher::Outcome::reused, Failure::none};
                }
                // Native residency loss is cache unavailability, not source
                // mutation. Drop only the idle native payload, not its watcher.
                publisher->mState->evict();
            }
            if(preparation_frame&&at->second.last_prepare_attempt&&
                *preparation_frame<=*at->second.last_prepare_attempt)
            {++mImpl->counts.misses;Result result;result.defer_preparation=true;return result;}
            if(preparation_frame&&publisher->mSourceWitness)
            {
                const auto& state=publisher->mSourceWitness->state;
                if(at->second.source_stability.deferUnstableRevision(*preparation_frame,
                        ALNativeCohortCompilationAdmission::ProducerStamp{state.identity(),state.revision()}))
                {++mImpl->counts.misses;Result result;result.defer_preparation=true;return result;}
            }
        }
        else publisher->invalidate();
    }
    ++mImpl->counts.misses; return {};
}

ALMetalShadowMembershipCache::Result ALMetalShadowMembershipCache::seal(
    const std::shared_ptr<ALNativePublication::State>& owner, std::uint64_t flavor,
    std::span<const Source> sources, std::span<const ALMetalShadowMembershipRange> ranges,
    ALNativeSourceWorkingSet& working_set, ALNativeFrameBudget& budget,
    std::uint64_t& serial, std::uint32_t frame, ALNativeFrameBudget::Frame oldest)
{
    if (!owner) return {{}, {}, Failure::invalid_source};
    const Impl::Key key{owner->identity(),flavor}; const auto revision = owner->revision();
    try
    {
        auto at = mImpl->entries.find(key);
        std::shared_ptr<ALMetalShadowMembershipPublisher> publisher;
        if (at == mImpl->entries.end())
        {
            if (!mImpl->limits.maximum_entries || Impl::entry_charge > mImpl->limits.maximum_metadata_bytes)
                return {{}, {}, Failure::bounds};
            auto reservation = std::make_shared<ALMetalShadowMembershipReservation>(mImpl->accounting,Impl::entry_charge);
            // Retiring idle entries may release debt. Actual CPU readers keep
            // their own reservations, so eviction never manufactures credit.
            while (!reservation->acquired && !mImpl->entries.empty())
            {
                auto victim=mImpl->entries.begin();
                for(auto candidate=mImpl->entries.begin();candidate!=mImpl->entries.end();++candidate)
                    if(candidate->second.last_used<victim->second.last_used)victim=candidate;
                mImpl->erase(victim);
                reservation=std::make_shared<ALMetalShadowMembershipReservation>(mImpl->accounting,Impl::entry_charge);
            }
            if (!reservation->acquired) return {{}, {}, Failure::cpu_residency};
            publisher = std::make_shared<ALMetalShadowMembershipPublisher>(mImpl->limits.publication);
            publisher->mAccounting=mImpl->accounting;
            mImpl->entries.emplace(key, Impl::Entry{std::move(reservation),owner,revision,++mImpl->clock,publisher,oldest,{},{}});
            if (!mImpl->trim(&key)) { mImpl->entries.erase(key); return {{}, {}, Failure::bounds}; }
        }
        else
        {
            publisher = at->second.publisher;
            if (!sameOwner(at->second.owner.lock(), owner) || at->second.owner_revision != revision)
                publisher->invalidate();
            at->second.owner = owner; at->second.owner_revision = revision;
            at->second.last_used = ++mImpl->clock;
        }
        at=mImpl->entries.find(key);
        if(!at->second.first_unserved)at->second.first_unserved=oldest;
        else at->second.first_unserved=std::min(*at->second.first_unserved,oldest);
        const auto pending_age=*at->second.first_unserved;
        // Existing group draw-map invalidation is the authoritative membership
        // edge. It is separate from every source backing-generation edge.
        owner->observe(publisher->mOwnerDependencies);
        // Scheduling only: direct seal/nested transactions remain valid.
        // Opted-in current misses in this or a regressing frame can avoid
        // another pending vector/union attempt after denial or transient work.
        at=mImpl->entries.find(key);
        if(at!=mImpl->entries.end()&&at->second.publisher==publisher&&at->second.owner_revision==revision)
            at->second.last_prepare_attempt=budget.snapshot().frame;
        auto result = publisher->seal(sources,ranges,working_set,budget,serial,frame,pending_age);
        mImpl->counts.resource_unions += result.sealed_resource_union ? 1u : 0u;
        if (owner->revision() != revision)
        {
            // The observed owner already withdrew our stale attempt BEFORE
            // its mutation. Reentrant work may now have installed a newer
            // valid publication in this SAME publisher: never clear it here.
            return {{}, {}, Failure::changed_source};
        }
        at = mImpl->entries.find(key); // Retain/eviction callbacks may reenter.
        if (at == mImpl->entries.end() || at->second.publisher != publisher ||
            at->second.owner_revision != revision)
        {
            // Another entry owns this key, or our entry was removed. Its
            // current publication belongs to that transaction. A successful
            // old result remains a transient reader, never an idle cache.
            if (result) result.outcome = ALMetalShadowMembershipPublisher::Outcome::transient;
        }
        else if (!mImpl->trim(&key))
        {
            // A cache decline may withdraw ONLY the exact payload this call
            // successfully installed, not a newer nested state/publication.
            if (result && sameOwner(publisher->mState->publication(),result.membership))
                publisher->mState->evict();
            at = mImpl->entries.find(key);
            if (at != mImpl->entries.end() && at->second.publisher == publisher &&
                at->second.owner_revision == revision && !publisher->mState->publication()) mImpl->entries.erase(at);
            if (result) result.outcome = ALMetalShadowMembershipPublisher::Outcome::transient;
        }
        else if(result && (result.outcome==ALMetalShadowMembershipPublisher::Outcome::retained ||
                result.outcome==ALMetalShadowMembershipPublisher::Outcome::reused) &&
                sameOwner(publisher->mState->publication(),result.membership))
        {
            // Only this exact successful current publication retires pending
            // work. A stale outer attempt cannot reset a nested new request.
            at=mImpl->entries.find(key);
            if(at!=mImpl->entries.end() && at->second.publisher==publisher &&
                at->second.owner_revision==revision)at->second.first_unserved.reset();
        }
        at=mImpl->entries.find(key);
        if(result && at!=mImpl->entries.end() && at->second.publisher==publisher &&
            at->second.owner_revision==revision && publisher->mSourceWitness)
        {
            const auto& state=publisher->mSourceWitness->state;
            // Prime the newly successful set at the exact outer budget frame.
            // Native cache withdrawal alone can then progress on a later one.
            (void)at->second.source_stability.deferUnstableRevision(budget.snapshot().frame,
                ALNativeCohortCompilationAdmission::ProducerStamp{state.identity(),state.revision()});
        }
        return result;
    }
    catch (const std::bad_alloc&) { return {{}, {}, Failure::allocation}; }
}
void ALMetalShadowMembershipCache::clear() noexcept
{
    for (auto& [key, entry] : mImpl->entries) { (void)key; entry.publisher->invalidate(); }
    mImpl->entries.clear();
}
ALMetalShadowMembershipCache::Stats ALMetalShadowMembershipCache::stats() const noexcept
{
    auto result = mImpl->counts; result.entries = mImpl->entries.size();
    result.metadata_bytes = mImpl->bytes(); return result;
}
std::shared_ptr<const ALMetalShadowMembershipAccounting> ALMetalShadowMembershipCache::accounting() const noexcept
{ return mImpl->accounting; }

bool alMetalGatherShadowMembershipRange(const ALMetalShadowMembership& membership,
    std::size_t range_index, std::span<const ALMetalShadowMembershipViewInput> facts,
    std::span<ALMetalShadowPrepareInstance> scratch,
    std::span<std::array<float,16>> texture_scratch, std::uint32_t texture_base_slot,
    std::uint64_t epoch, std::size_t& texture_count, std::string* error)
{
    texture_count=0;
    const auto fail = [&](const char* message) {
        std::fill(scratch.begin(), scratch.end(), ALMetalShadowPrepareInstance{});
        if (error) *error = message;
        return false;
    };
    if (range_index >= membership.ranges().size()) return fail("Shadow membership range is absent");
    const auto& range = membership.ranges()[range_index];
    if (facts.size() != range.count || scratch.size() != range.count)
        return fail("Shadow membership view span mismatch");
    std::size_t matrices = 0;
    for (std::size_t at = 0; at < range.count; ++at)
    {
        const auto& member = membership.members()[range.first + at]; const auto& fact = facts[at];
        if (fact.source_generation != member.record->generation())
            return fail("Shadow membership view generation is stale or reordered");
        auto& input = scratch[at]; input = {};
        input.source = member.record.get(); input.source_generation = fact.source_generation;
        input.palette = fact.palette; input.epoch = epoch;
        input.model = fact.model; input.has_model = fact.has_model;
        if (fact.has_texture_matrix)
        {
            if (member.record->prototype().coverage == ALMetalWorldSpotShadowCoverage::opaque ||
                member.record->prototype().texture_count > 1)
                return fail("Shadow membership UV override violates source policy");
            if(matrices>=std::numeric_limits<std::uint32_t>::max()-texture_base_slot)
                return fail("Shadow membership UV context slot overflows");
            if(matrices>=texture_scratch.size())return fail("Shadow membership UV scratch is insufficient");
            texture_scratch[matrices] = fact.texture_matrix;
            input.texture_matrix_slot = texture_base_slot+static_cast<std::uint32_t>(matrices++);
        }
    }
    texture_count=matrices;return true;
}

bool alMetalPrepareShadowMembershipRange(const ALMetalShadowMembership& membership,
    std::size_t range_index, std::span<const ALMetalShadowMembershipViewInput> facts,
    const ALMetalShadowPrepareContext& context, std::span<ALMetalShadowPrepareInstance> scratch,
    std::span<std::array<float,16>> texture_scratch, std::span<ALMetalWorldSpotShadowBatch> output,
    ALNativeFrameBudget& budget, std::uint64_t epoch, const std::atomic<std::uint64_t>* current_epoch,
    bool allow_dispatch, ALMetalShadowPrepareStats* stats, std::string* error)
{
    std::size_t matrices=0;
    if(output.size()!=facts.size() || !alMetalGatherShadowMembershipRange(membership,range_index,
            facts,scratch,texture_scratch,0,epoch,matrices,error))
    {
        std::fill(output.begin(),output.end(),ALMetalWorldSpotShadowBatch{});
        std::fill(scratch.begin(),scratch.end(),ALMetalShadowPrepareInstance{});
        if(stats)*stats={};
        if(error && output.size()!=facts.size())*error="Shadow membership output span mismatch";
        return false;
    }
    auto prepared_context = context;
    // Caller context UVs are not selected implicitly; every current animated
    // override is explicitly copied from this exact view's owner facts.
    prepared_context.texture_matrices = texture_scratch.first(matrices);
    if (!alMetalPrepareShadowBatches(scratch, prepared_context, output, budget,
            epoch, current_epoch, allow_dispatch, stats, error)) return false;
    if (!allow_dispatch)
        for (auto& batch : output)
            batch.borrowPreparedInputs(nullptr,batch.skinPaletteRecord(),batch.coverageDataIdentity());
    return true;
}
