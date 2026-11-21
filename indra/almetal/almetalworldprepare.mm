/**
 * @file almetalworldprepare.mm
 * @brief Immutable world packet ownership for scalar native encoding.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldprepare.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <unordered_map>

namespace
{
void fail(std::string* error, const char* message)
{
    if (error) *error = message;
}
struct NativeOwners
{
    std::vector<std::shared_ptr<const void>> owners;
};
}

struct ALMetalWorldViewInputs::Impl
{
    ALMetalWorldGBufferScene scene;
    std::vector<ALMetalWorldSimpleBatch> batches;
    std::vector<ALMetalWorldSourceReference> references;
    std::vector<ALMetalWorldDrawIndex> draw_order;
    std::vector<ALMetalWorldSourceCohortView> cohorts;
    std::vector<ALMetalWorldVisibilityQuery> queries;
    std::shared_ptr<const void> resources;
    std::vector<std::vector<float>> poses;
    std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> palette_records;
    std::vector<std::shared_ptr<const ALMetalBodyPaletteRecord>> body_palette_records;
    std::vector<std::shared_ptr<const ALMetalWorldAnimatedSource>> animated_sources;
    std::array<float, 4> coverage_fill{};
    std::uint64_t epoch = 0;
};

ALMetalWorldViewInputs::ALMetalWorldViewInputs(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalWorldViewInputs::~ALMetalWorldViewInputs() = default;
const ALMetalWorldGBufferScene& ALMetalWorldViewInputs::scene() const noexcept { return mImpl->scene; }
std::uint64_t ALMetalWorldViewInputs::epoch() const noexcept { return mImpl->epoch; }
std::shared_ptr<const void> ALMetalWorldViewInputs::resourceLease() const noexcept { return mImpl->resources; }

bool ALMetalWorldViewInputs::reclaimCpuStorage(
    std::shared_ptr<const ALMetalWorldViewInputs>& input,
    std::vector<ALMetalWorldSimpleBatch>& empty_batches,
    std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept
{
    if (!input || input.use_count() != 1 || !empty_batches.empty() || !empty_queries.empty()) return false;
    auto& impl = *input->mImpl;
    // The raw compatibility API must not silently discard compact records.
    if (!impl.references.empty() || !impl.draw_order.empty() || !impl.cohorts.empty()) return false;
    empty_batches.swap(impl.batches);
    empty_queries.swap(impl.queries);
    empty_batches.clear();
    empty_queries.clear();
    input.reset();
    return true;
}

bool ALMetalWorldViewInputs::reclaimCpuStorage(
    std::shared_ptr<const ALMetalWorldViewInputs>& input,
    std::vector<ALMetalWorldSimpleBatch>& empty_batches,
    std::vector<ALMetalWorldSourceReference>& empty_references,
    std::vector<ALMetalWorldDrawIndex>& empty_draw_order,
    std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept
{
    if (!input || input.use_count() != 1 || !empty_batches.empty() || !empty_references.empty() ||
        !empty_draw_order.empty() || !empty_queries.empty()) return false;
    auto& impl = *input->mImpl;
    if (!impl.cohorts.empty()) return false;
    empty_batches.swap(impl.batches);
    empty_references.swap(impl.references);
    empty_draw_order.swap(impl.draw_order);
    empty_queries.swap(impl.queries);
    empty_batches.clear(); empty_references.clear(); empty_draw_order.clear(); empty_queries.clear();
    input.reset();
    return true;
}

bool ALMetalWorldViewInputs::reclaimCpuStorage(
    std::shared_ptr<const ALMetalWorldViewInputs>& input,
    std::vector<ALMetalWorldSimpleBatch>& empty_batches,
    std::vector<ALMetalWorldSourceReference>& empty_references,
    std::vector<ALMetalWorldDrawIndex>& empty_draw_order,
    std::vector<ALMetalWorldSourceCohortView>& empty_cohorts,
    std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept
{
    if(!input || input.use_count()!=1 || !empty_batches.empty() || !empty_references.empty() ||
       !empty_draw_order.empty() || !empty_cohorts.empty() || !empty_queries.empty())return false;
    auto& impl=*input->mImpl;
    empty_batches.swap(impl.batches);empty_references.swap(impl.references);empty_draw_order.swap(impl.draw_order);
    empty_cohorts.swap(impl.cohorts);empty_queries.swap(impl.queries);
    empty_batches.clear();empty_references.clear();empty_draw_order.clear();empty_cohorts.clear();empty_queries.clear();
    input.reset();return true;
}

std::shared_ptr<const ALMetalWorldViewInputs> ALMetalWorldViewInputs::adopt(
    ALMetalWorldGBufferScene scene, std::vector<ALMetalWorldSimpleBatch> batches,
    std::vector<ALMetalWorldVisibilityQuery> queries,
    std::shared_ptr<const void> resource_owner, std::uint64_t epoch, std::string* error)
try
{
    if (scene.reference_count || scene.draw_order_count || scene.cohort_count)
    {
        fail(error, "Raw world adoption cannot discard compact references or order");
        return {};
    }
    auto cpu_owner = std::move(scene.cpu_source_owner);
    return adopt(std::move(scene), std::move(batches), {}, {}, std::move(queries),
        std::move(cpu_owner), std::move(resource_owner), epoch, error);
}
catch (const std::bad_alloc&)
{
    if (error) error->clear();
    return {};
}

std::shared_ptr<const ALMetalWorldViewInputs> ALMetalWorldViewInputs::adopt(
    ALMetalWorldGBufferScene scene, std::vector<ALMetalWorldSimpleBatch> batches,
    std::vector<ALMetalWorldSourceReference> references,
    std::vector<ALMetalWorldDrawIndex> draw_order,
    std::vector<ALMetalWorldVisibilityQuery> queries,
    std::shared_ptr<const void> cpu_description_owner,
    std::shared_ptr<const void> native_resource_owner, std::uint64_t epoch, std::string* error)
{
    if(scene.cohort_count)
    {fail(error,"Compact world adoption cannot discard cohort storage");return {};}
    return adopt(std::move(scene),std::move(batches),std::move(references),std::move(draw_order),{},
        std::move(queries),std::move(cpu_description_owner),std::move(native_resource_owner),epoch,error);
}

std::shared_ptr<const ALMetalWorldViewInputs> ALMetalWorldViewInputs::adopt(
    ALMetalWorldGBufferScene scene,std::vector<ALMetalWorldSimpleBatch> batches,
    std::vector<ALMetalWorldSourceReference> references,std::vector<ALMetalWorldDrawIndex> draw_order,
    std::vector<ALMetalWorldSourceCohortView> cohorts,std::vector<ALMetalWorldVisibilityQuery> queries,
    std::shared_ptr<const void> cpu_description_owner,std::shared_ptr<const void> native_resource_owner,
    std::uint64_t epoch,std::string* error)
try
{
    if ((!batches.empty() || !references.empty() || scene.destination) && !native_resource_owner)
    {
        fail(error, "Owned world inputs require an immutable resource lease");
        return {};
    }
    if (!references.empty() && !cpu_description_owner)
    {
        fail(error, "Compact world references require immutable CPU description ownership");
        return {};
    }
    auto impl = std::make_unique<Impl>();
    impl->batches = std::move(batches);
    impl->references = std::move(references);
    impl->draw_order = std::move(draw_order);
    impl->cohorts = std::move(cohorts);
    impl->queries = std::move(queries);
    impl->resources = std::move(native_resource_owner);
    for(const auto& reference:impl->references)if(reference.animated_source)
    {
        auto owner=reference.animated_source->owner();auto native=reference.animated_source->nativeLease();
        if(!owner||!native||!reference.animated_palette){fail(error,"Animated input ownership is invalid");return {};}
        impl->animated_sources.push_back(std::move(owner));
        impl->palette_records.push_back(reference.animated_palette->shared_from_this());
    }
    if(!impl->cohorts.empty() || !impl->animated_sources.empty())
    {
        auto native=std::make_shared<NativeOwners>();native->owners.reserve(impl->cohorts.size()+1);
        std::set<std::shared_ptr<const void>,std::owner_less<std::shared_ptr<const void>>> unique;
        if(impl->resources){unique.insert(impl->resources);native->owners.push_back(impl->resources);}
        for(const auto& view:impl->cohorts)
            if(view.native_owner && unique.insert(view.native_owner).second)native->owners.push_back(view.native_owner);
        for(const auto& source:impl->animated_sources)
        {auto lease=source->nativeLease();if(lease&&unique.insert(lease).second)native->owners.push_back(std::move(lease));}
        impl->resources=std::move(native);
    }
    impl->epoch = epoch;
    // The caller's transient array addresses are deliberately replaced.
    scene.batches = impl->batches.data();
    scene.batch_count = impl->batches.size();
    scene.references = impl->references.data();
    scene.reference_count = impl->references.size();
    scene.draw_order = impl->draw_order.data();
    scene.draw_order_count = impl->draw_order.size();
    scene.cohorts = impl->cohorts.data();
    scene.cohort_count = impl->cohorts.size();
    scene.visibility_queries = impl->queries.data();
    scene.visibility_query_count = impl->queries.size();
    scene.cpu_source_owner = std::move(cpu_description_owner);
    scene.source_owner = impl->resources;
    if (scene.opaque_coverage_fill)
    {
        std::memcpy(impl->coverage_fill.data(), scene.opaque_coverage_fill, sizeof(impl->coverage_fill));
        scene.opaque_coverage_fill = impl->coverage_fill.data();
    }
    // G-buffer encoding never consumes these forward/lighting owners.
    // Do not carry mutable viewer pointers into an owned input packet.
    scene.reflection_probes = nullptr;
    scene.hero_probe = nullptr;
    scene.screen_space_reflections = nullptr;
    scene.legacy_alpha_lighting = nullptr;
    scene.directional_shadows = nullptr;
    impl->scene = scene;
    if (!alMetalValidateWorldView(impl->scene, error)) return {};

    struct Pose { std::size_t slot, count; std::uint32_t joints; };
    std::unordered_map<const float*, Pose> snapshots;
    std::set<const ALMetalAttachmentPaletteRecord*> palette_records;
    std::set<const ALMetalBodyPaletteRecord*> body_palette_records;
    for (auto& batch : impl->batches)
    {
        if(batch.body_palette_record)
        {
            if(!batch.matchesBodyPaletteRecord())
            {fail(error,"Owned world body record does not match its pose");return {};}
            if(body_palette_records.insert(batch.body_palette_record).second)
            {
                auto owner=batch.body_palette_record->owner();
                if(!owner){fail(error,"Owned world body record has no immutable owner");return {};}
                impl->body_palette_records.push_back(std::move(owner));
            }
            continue;
        }
        if (batch.skin_palette_record)
        {
            if (!batch.matchesSkinPaletteRecord())
            {
                fail(error, "Owned world attachment record does not match its pose");
                return {};
            }
            if (palette_records.insert(batch.skin_palette_record).second)
                impl->palette_records.push_back(batch.skin_palette_record->shared_from_this());
            continue;
        }
        if (!batch.skin_palette)
        {
            if (batch.skin_palette_float_count || batch.skin_joint_count)
            {
                fail(error, "Owned world pose has no values");
                return {};
            }
            continue;
        }
        const std::size_t expected = batch.avatar_skinning
            ? 45 * 4 : 4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
        if (batch.skin_joint_count == 0 || batch.skin_joint_count > 110 ||
            (batch.avatar_skinning && batch.skin_joint_count != 15) ||
            batch.skin_palette_float_count != expected)
        {
            fail(error, "Owned world pose length is invalid");
            return {};
        }
        const auto found = snapshots.find(batch.skin_palette);
        std::size_t slot;
        if (found == snapshots.end())
        {
            slot = impl->poses.size();
            impl->poses.emplace_back(batch.skin_palette, batch.skin_palette + expected);
            snapshots.emplace(batch.skin_palette, Pose{slot, expected, batch.skin_joint_count});
        }
        else
        {
            if (found->second.count != expected || found->second.joints != batch.skin_joint_count)
            {
                fail(error, "Owned world pose aliases inconsistent layouts");
                return {};
            }
            slot = found->second.slot;
        }
        batch.skin_palette = impl->poses[slot].data();
    }
    return std::shared_ptr<const ALMetalWorldViewInputs>(new ALMetalWorldViewInputs(std::move(impl)));
}
catch (const std::bad_alloc&)
{
    if (error) error->clear();
    return {};
}
catch (const std::bad_weak_ptr&)
{
    fail(error, "Owned world attachment record has no shared owner");
    return {};
}
