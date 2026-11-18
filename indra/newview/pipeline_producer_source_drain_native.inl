/**
 * @file pipeline_producer_source_drain_native.inl
 * @brief Bounded owner preparation of completed producer source recipes.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace ALSceneProducerDrainNative
{
struct Stats
{
    U64 frames = 0, candidates = 0, rows = 0, requests = 0;
    U64 warm = 0, prepared = 0, completed = 0, deferred = 0;
    U64 stale = 0, incomplete = 0, refreshed = 0, allocation_failures = 0;
    U64 wall_ns = 0;
};
inline Stats statistics;
inline std::optional<ALNativeFrameBudget::Frame> last_frame;
inline std::size_t scan_cursor = 0;

// Required serial rendering remains available on every declined/partial path.
// The input is the current owner cull result, not a retained view or raw worker
// packet. Only the bounded LLPointer pins below cross a factory callback.
template<typename Iterator, typename Policy, typename PinGroup, typename PinDraw, typename SelectFlavor>
void drain(Iterator begin, Iterator end, const Policy& policy,
    ALNativeSourceWorkingSet& working_set, U64& serial, U32 display_frame,
    ALNativeFrameBudget& budget, PinGroup&& pin_group, PinDraw&& pin_draw, SelectFlavor&& select_flavor)
try
{
    const auto frame = budget.snapshot().frame;
    if (last_frame && frame <= *last_frame) return;
    last_frame = frame;
    ++statistics.frames;
    const auto started = std::chrono::steady_clock::now();
    struct Timing
    {
        std::chrono::steady_clock::time_point started;
        ~Timing()
        {
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started).count();
            if (ns > 0) statistics.wall_ns += static_cast<U64>(ns);
        }
    } timing{started};
    if (begin == end || budget.snapshot().draining) return;
    constexpr std::size_t maximum_candidates = 256, maximum_rows = 256;
    const auto count = static_cast<std::size_t>(end - begin);
    const auto first = scan_cursor % count;
    const auto considered = std::min(count, maximum_candidates);
    scan_cursor = (first + considered) % count;
    using GroupOwner = decltype(pin_group(*begin));
    using DrawOwner = decltype(pin_draw(static_cast<LLDrawInfo*>(nullptr)));
    std::vector<GroupOwner> candidates;
    candidates.reserve(considered);
    const auto signature = policy.signature();
    for (std::size_t n = 0; n < considered; ++n)
    {
        auto* group = *(begin + (first + n) % count);
        if (!group || group->isDead()) continue;
        const auto& work = group->metalSourceWork();
        if (!work || !group->sourceWorkCurrent(*work)) continue;
        const auto& state = group->metalSourceWorkState();
        if (group->mMetalSourcePreparationPolicy != signature)
        {
            state->invalidate();
            group->mMetalSourcePreparationPolicy = signature;
        }
        if (!state->dirty()) continue;
        group->mMetalSourcePreparation.notePending(frame);
        candidates.push_back(pin_group(group));
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b)
    {
        return a->mMetalSourcePreparation.priority(a->metalSourceWorkState()->identity()) <
            b->mMetalSourcePreparation.priority(b->metalSourceWorkState()->identity());
    });
    statistics.candidates += candidates.size();
    std::size_t remaining = maximum_rows;
    for (const auto& group : candidates)
    {
        if (!remaining) break;
        const auto work = group->metalSourceWork();
        const auto state = group->metalSourceWorkState();
        if (!work || !group->sourceWorkCurrent(*work) || !state->dirty()) continue;
        const auto revision = state->revision();
        const auto current = [&]
        {
            return group->sourceWorkCurrent(*work) && group->metalSourceWorkState() == state &&
                state->revision() == revision && group->mMetalSourcePreparationPolicy == signature;
        };
        if (group->mMetalSourcePreparationIdentity != state->identity() ||
            group->mMetalSourcePreparationRevision != revision)
        {
            group->mMetalSourcePreparationIdentity = state->identity();
            group->mMetalSourcePreparationRevision = revision;
            group->mMetalSourcePreparationCursor = 0;
        }
        const auto rows = work->rows();
        auto cursor = std::min(group->mMetalSourcePreparationCursor, rows.size());
        const auto stop = cursor + std::min(remaining, rows.size() - cursor);
        std::vector<DrawOwner> pins;
        std::vector<ALScenePublicationNative::ProducerWorldRequest> requests;
        pins.reserve(stop - cursor); requests.reserve(stop - cursor);
        bool refresh = false;
        for (; cursor < stop; ++cursor)
        {
            --remaining; ++statistics.rows;
            if (!current()) break;
            const auto& row = rows[cursor];
            auto producer = row.producer.lock();
            if (!row.source) continue;
            if (!producer || producer->identity() != row.identity) { refresh = true; break; }
            auto pin = pin_draw(row.source);
            if (!pin) { refresh = true; break; }
            if (pin->metalSourceLayoutRevision() != row.layout_revision ||
                pin->metalSourcePublication() != producer) { refresh = true; break; }
            auto flavor = select_flavor(row.pass, *pin, policy);
            if (!flavor || !pin->mCount) continue;
            requests.push_back({ALScenePublicationNative::producerSource(*pin), *flavor});
            pins.push_back(std::move(pin));
        }
        if (refresh)
        {
            if (current())
            {
                state->invalidate();
                group->mMetalSourcePreparationCursor = 0;
                statistics.refreshed += group->publishMetalSourceWork(true);
            }
            ++statistics.stale;
            continue;
        }
        if (!current()) { ++statistics.stale; continue; }
        statistics.requests += requests.size();
        const auto logical_bytes = pins.size() * sizeof(pins.front()) +
            requests.size() * sizeof(requests.front());
        const auto prepared = ALScenePublicationNative::prepareProducerSources(requests, {},
            working_set, serial, display_frame, group->mMetalSourcePreparation, budget, logical_bytes,
            current, [](const auto&, auto&) { return false; });
        statistics.warm += prepared.warm; statistics.prepared += prepared.prepared;
        if (!prepared)
        {
            if (prepared.status == ALScenePublicationNative::ProducerPreparationStatus::deferred) ++statistics.deferred;
            else if (prepared.status == ALScenePublicationNative::ProducerPreparationStatus::stale) ++statistics.stale;
            else ++statistics.incomplete;
            continue;
        }
        if (!current()) { ++statistics.stale; continue; }
        group->mMetalSourcePreparationCursor = cursor;
        if (cursor == rows.size() && state->publish(revision, work)) ++statistics.completed;
    }
}
catch (const std::bad_alloc&) { ++statistics.allocation_failures; }
}
