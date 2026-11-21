/**
 * @file pipeline_producer_capability_native.inl
 * @brief Separately admitted bounds for one proven heavy material pass.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace ALSceneProducerDrainNative
{
inline U64 capability_requests=0, capability_completed=0, capability_deferred=0;

template<typename Group, typename Policy>
bool needsCapability(const Group& group, const Policy& policy) noexcept
{
    const auto& state=group.metalSourceWorkState();
    if(!policy.heavy_bounds || !state || state->dirty() ||
       group.mMetalSourcePreparationIdentity!=state->identity() ||
       group.mMetalSourcePreparationRevision!=state->revision() ||
       group.mMetalSourcePreparationPolicy!=policy.signature() ||
       !alMetalWorldNeedsHeavySourceBounds(group.mMetalSourceCapabilitySummary.input())) return false;
    return !group.mMetalSourceCapabilityComplete ||
        group.mMetalSourceCapabilityIdentity!=state->identity() ||
        group.mMetalSourceCapabilityRevision!=state->revision() ||
        group.mMetalSourceCapabilityPolicy!=policy.signature();
}

template<typename Iterator, typename Policy, typename PinGroup, typename PinDraw, typename SelectFlavor>
void drainCapabilities(Iterator begin, Iterator end, const Policy& policy,
    ALNativeSourceWorkingSet& working_set, U64& serial, U32 display_frame,
    ALNativeFrameBudget& budget, PinGroup&& pin_group, PinDraw&& pin_draw,
    SelectFlavor&& select_flavor, bool early)
try
{
    if(!policy.heavy_bounds || active || !last_frame || begin==end || !rows_remaining ||
       (early && !producer_rows_remaining)) return;
    const auto observed=budget.snapshot();
    if(!observed.initialized || observed.draining || observed.frame!=*last_frame) return;
    active=true;
    struct ActiveScope { ~ActiveScope(){active=false;} } active_scope;
    constexpr std::size_t maximum_candidates=256;
    const auto count=static_cast<std::size_t>(end-begin);
    const auto first=count>maximum_candidates?scan_cursor%count:0;
    const auto considered=std::min(count,maximum_candidates);
    using GroupOwner=decltype(pin_group(*begin));
    using DrawOwner=decltype(pin_draw(static_cast<LLDrawInfo*>(nullptr)));
    std::vector<GroupOwner> candidates;
    for(std::size_t n=0;n<considered;++n)
    {
        auto* group=*(begin+(first+n)%count);
        if(!group || group->isDead()) continue;
        const auto& work=group->metalSourceWork();
        if(!work || !group->sourceWorkCurrent(*work) || !needsCapability(*group,policy)) continue;
        group->mMetalSourceCapabilityPreparation.notePending(observed.frame);
        if(candidates.empty()) candidates.reserve(considered);
        candidates.push_back(pin_group(group));
    }
    std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b)
    { return a->mMetalSourceCapabilityPreparation.priority(a->metalSourceWorkState()->identity()) <
             b->mMetalSourceCapabilityPreparation.priority(b->metalSourceWorkState()->identity()); });
    auto remaining=early?std::min(rows_remaining,producer_rows_remaining):rows_remaining;
    for(const auto& group:candidates)
    {
        if(!remaining) break;
        const auto work=group->metalSourceWork();
        const auto state=group->metalSourceWorkState();
        if(!work || !group->sourceWorkCurrent(*work) || !needsCapability(*group,policy)) continue;
        const auto revision=state->revision(), signature=policy.signature();
        const auto selected_pass=group->mMetalSourceCapabilitySummary.pass;
        const auto current=[&]
        { return group->sourceWorkCurrent(*work) && group->metalSourceWorkState()==state &&
            state->revision()==revision && !state->dirty() &&
            group->mMetalSourcePreparationPolicy==signature &&
            group->mMetalSourceCapabilitySummary.pass==selected_pass; };
        if(group->mMetalSourceCapabilityIdentity!=state->identity() ||
           group->mMetalSourceCapabilityRevision!=revision ||
           group->mMetalSourceCapabilityPolicy!=signature)
        {
            group->mMetalSourceCapabilityIdentity=state->identity();
            group->mMetalSourceCapabilityRevision=revision;
            group->mMetalSourceCapabilityPolicy=signature;
            group->mMetalSourceCapabilityCursor=0;
            group->mMetalSourceCapabilityComplete=false;
        }
        const auto rows=work->rows();
        auto cursor=std::min(group->mMetalSourceCapabilityCursor,rows.size());
        const auto stop=cursor+std::min(remaining,rows.size()-cursor);
        std::vector<DrawOwner> pins;
        std::vector<ALScenePublicationNative::ProducerWorldRequest> requests;
        bool invalid=false;
        for(;cursor<stop;++cursor)
        {
            --remaining;--rows_remaining;if(early)--producer_rows_remaining;
            if(!current()){invalid=true;break;}
            const auto& row=rows[cursor];
            if(row.pass!=selected_pass || !row.source) continue;
            const auto producer=row.producer.lock();
            if(!producer || producer->identity()!=row.identity){invalid=true;break;}
            auto pin=pin_draw(row.source);
            if(!pin || pin->metalSourceLayoutRevision()!=row.layout_revision ||
               pin->metalSourcePublication()!=producer){invalid=true;break;}
            auto flavor=select_flavor(row.pass,*pin,policy);
            const auto* geometry=pin->mVertexBuffer.get();
            if(!flavor || flavor->rigged || !flavor->hasResourceConsumer() || pin->mTextureMatrix ||
               !geometry || !geometry->hasPublishedMetalVertexBuffer() ||
               !geometry->hasPublishedMetalIndexBuffer()){invalid=true;break;}
            flavor->cohort_source=flavor->cohort_bounds=true;
            flavor->animated_source=flavor->stabilize_animated_direct=false;
            requests.push_back({ALScenePublicationNative::producerSource(*pin),*flavor});
            pins.push_back(std::move(pin));
        }
        if(invalid || !current()){++statistics.stale;continue;}
        if(!requests.empty())
        {
            ++capability_requests;
            const auto logical_bytes=pins.size()*sizeof(pins.front())+requests.size()*sizeof(requests.front());
            const auto prepared=ALScenePublicationNative::prepareProducerSources(requests,{},working_set,serial,
                display_frame,group->mMetalSourceCapabilityPreparation,budget,logical_bytes,current,
                [](const auto&,auto&){return false;});
            if(!prepared){++capability_deferred;continue;}
        }
        if(!current()){++statistics.stale;continue;}
        group->mMetalSourceCapabilityCursor=cursor;
        if(cursor==rows.size())
        { group->mMetalSourceCapabilityComplete=true;++capability_completed; }
    }
}
catch(const std::bad_alloc&){++statistics.allocation_failures;}

template<typename Iterator, typename Policy, typename PinGroup, typename PinDraw, typename SelectFlavor>
void drain(Iterator begin, Iterator end, const Policy& policy,
    ALNativeSourceWorkingSet& working_set, U64& serial, U32 display_frame,
    ALNativeFrameBudget& budget, PinGroup&& pin_group, PinDraw&& pin_draw,
    SelectFlavor&& select_flavor, bool early=false)
{
    drainBase(begin,end,policy,working_set,serial,display_frame,budget,pin_group,pin_draw,select_flavor,early);
    drainCapabilities(begin,end,policy,working_set,serial,display_frame,budget,pin_group,pin_draw,select_flavor,early);
}
}
