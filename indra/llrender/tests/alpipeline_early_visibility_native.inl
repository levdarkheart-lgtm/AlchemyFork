/**
 * @file alpipeline_early_visibility_native.inl
 * @brief Actual static producers before per-view capture: early native classification.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <cstdlib>
#include <stdexcept>

namespace
{
namespace ActualEarlyVisibility
{
namespace Registry = Inputs::ALSceneGroupPublicationNative::early;
using Ticket = std::shared_ptr<const ALMetalWorldEarlyVisibilityArguments>;
using Receipt = std::shared_ptr<const ALMetalCommandCompletion>;
void requireWork(bool ok, const char* reason)
{ if (!ok) throw std::runtime_error(reason); }
U64 now()
{ return U64(std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count()); }
void wait(const Receipt& receipt)
{
    requireWork(bool(receipt), "Native command has no completion receipt");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!receipt->isComplete() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    requireWork(receipt->isComplete(), "Native command did not complete");
}
struct RegistryScope
{
    RegistryScope() { Registry::enable(false); Registry::enable(true); }
    ~RegistryScope() { Registry::enable(false); }
};
ALMetalWorldSourceCohortView earlyView(const Registry::Candidate& candidate, std::size_t logical)
{
    ALMetalWorldSourceCohortView result{logical, 0, candidate.cohort,
        candidate.cpu_owner, candidate.native_owner, {}};
    LLMatrix4a model(candidate.region.model.data()), view, inverse, projection;
    requireWork(Inputs::buildMetalWorldDrawMatrices(&model, view, inverse, projection), "Early view matrix is singular");
    std::memcpy(result.view.modelview_projection, projection.getF32ptr(), 64);
    std::memcpy(result.view.modelview, view.getF32ptr(), 64);
    const auto* inv = inverse.getF32ptr(); const auto* values = view.getF32ptr();
    for (U32 column = 0; column < 3; ++column) for (U32 row = 0; row < 3; ++row)
    { result.view.normal_matrix[column][row] = inv[row * 4 + column];
      result.view.rigged_normal_matrix[column][row] = values[column * 4 + row]; }
    return result;
}
}
}

TEST_CASE("Actual early registry copies fresh regions without touching draw sources and rejects withdrawn generations",
          "[almetal][pipeline-buffer-publication][early-visibility-producer]")
{
    using namespace ActualEarlyVisibility;
    MaterialGroupPublication f(1); RegistryScope registry;
    f.flavor.cohort_bounds = true; f.flavor.native_resources = false;
    auto& info = f.materialDraw(true);
    auto packet = f.sealMaterial(); REQUIRE(packet);
    const std::array regions{f.region.snapshot()};
    const auto reads = f.reads();
    auto candidates = Registry::snapshot(regions);
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0].cohort == packet->cohort); CHECK(candidates[0].cpu_owner == packet);
    CHECK(candidates[0].state_identity == f.group->identity()); CHECK(f.reads() == reads);
    auto model = f.model; model.getF32ptr()[12] = .125f;
    REQUIRE(f.region.update(model, LLVector3(4.f, 0.f, 0.f)));
    const std::array rebased{f.region.snapshot()};
    auto fresh = Registry::snapshot(rebased); REQUIRE(fresh.size() == 1);
    CHECK(fresh[0].cohort == packet->cohort); CHECK(fresh[0].region.revision == rebased[0].revision);
    CHECK(fresh[0].region.model == rebased[0].model); CHECK(f.reads() == reads);
    f.group->evict(); CHECK(Registry::snapshot(rebased).empty());
    candidates.clear(); fresh.clear(); packet.reset();
    f.model = model; packet = f.sealMaterial(); REQUIRE(packet);
    REQUIRE(Registry::snapshot(rebased).size() == 1);
    const std::array<LLColor4U, 3> colors{LLColor4U(0,255,0,255), LLColor4U(0,255,0,255), LLColor4U(0,255,0,255)};
    info.mVertexBuffer->setColorData(colors.data()); info.mVertexBuffer->unmapBuffer();
    CHECK(Registry::snapshot(rebased).empty()); // The actual LLVB willWrite signal reaches the producer.
    packet = f.sealMaterial(); REQUIRE(packet);
    std::weak_ptr<const NativeGroup> weak_packet = packet;
    f.group->evict(); Inputs::sNativeSceneSources.clear(); packet.reset();
    CHECK(weak_packet.expired()); CHECK(Registry::snapshot(rebased).empty());
    CHECK(Registry::stats().resident_groups == 0);
}

TEST_CASE("Actual early native arguments retain no producer CPU graph after cache withdrawal",
          "[almetal][pipeline-buffer-publication][early-visibility-producer]")
{
    using namespace ActualEarlyVisibility;
    MaterialGroupPublication f(2); RegistryScope registry;
    f.flavor.cohort_bounds = true; f.flavor.native_resources = false;
    f.materialDraw(true); auto packet = f.sealMaterial(); REQUIRE(packet);
    const std::array regions{f.region.snapshot()};
    auto candidates = Registry::snapshot(regions); REQUIRE(candidates.size() == 1);
    std::vector<ALMetalWorldSourceCohortView> views{earlyView(candidates[0], 0)};
    auto table = ALMetalWorldVisibility::create(*f.device, &f.error); REQUIRE(table);
    auto view = table->makeView({}, views, views[0].cohort->size(), &f.error); REQUIRE(view);
    auto command = f.device->makeCommandBuffer("Actual early CPU graph abandonment", &f.error); REQUIRE(command);
    auto data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(data); data->beginFrame(1);
    auto ticket = ALMetalWorldVisibility::encodeEarly(*command, *view, &f.error, data.get());
    INFO(f.error); REQUIRE(ticket);
    std::weak_ptr<const NativeGroup> weak_packet = packet;
    std::weak_ptr<const ALMetalWorldSourceCohort> weak_cohort = packet->cohort;
    views.clear(); candidates.clear(); view.reset(); packet.reset();
    f.group->evict(); Inputs::sNativeSceneSources.clear();
    CHECK(weak_packet.expired()); CHECK(weak_cohort.expired());
    const auto committed = ALMetalDevice::committedCommandBufferCount();
    command.reset(); ticket.reset(); // Abandoned A must not publish or wait for a consumer.
    CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
    CHECK(Registry::snapshot(regions).empty());
}

namespace
{
namespace ActualEarlyVisibility
{
struct Group
{
    U32 family = 0, type = 0;
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    std::vector<Inputs::LLDrawInfo*> sources;
    std::shared_ptr<ALNativePublication::State> state = std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> dependencies;
    std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
    ALNativeCohortCompilationAdmission compilation;
};
struct Arm
{
    Inputs::LLPipeline pipeline;
    LLMatrix4a model = LLMatrix4a::identity();
    ALNativeRegionModel region;
    std::vector<std::unique_ptr<Group>> groups;
    std::array<Inputs::LLDrawInfo*, 2> holes{};
    ALNativeFrameBudget budget;
    std::unique_ptr<ALMetalWorldMaterialDataCache> material_cache;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::array<std::unique_ptr<ALMetalWorldGBufferRenderer>, 2> renderers;
    std::shared_ptr<ALMetalWorldVisibility> visibility;
    std::vector<ALMetalWorldVisibilityQuery> queries;
    ~Arm()
    {
        for (auto& group : groups)
        { group->state->invalidate(); for (auto* source : group->sources) source->state->invalidate(); }
        for (auto* hole : holes) if (hole) hole->state->invalidate();
    }
};
struct MaterialScope
{
    Arm& arm;
    std::unique_ptr<ALMetalWorldMaterialDataCache> previous;
    explicit MaterialScope(Arm& value) : arm(value)
    { previous.swap(Inputs::ALScenePublicationNative::material_data_cache);
      arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache); }
    ~MaterialScope()
    { arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);
      previous.swap(Inputs::ALScenePublicationNative::material_data_cache); }
};
struct Sample
{
    double owner_wall_ms = 0, settled_wall_ms = 0, gpu_A_ms = 0, gpu_B_ms = 0, gpu_envelope_ms = 0;
    U64 prefix_attempts = 0, prefix_commits = 0, prefix_denied = 0, prefix_ready_at_consumer = 0;
    U64 candidates = 0, candidate_rows = 0, snapshot_copy_bytes = 0, pre_cull_batches = 0;
    U64 first_visibility_creation_ns = 0;
    U64 raw_draws = 0, cohort_draws = 0, cohort_groups = 0, publication_debt_groups = 0;
    U64 native_publication_bytes = 0, group_builds = 0, compile_admitted = 0, compile_denied = 0, stability_deferred = 0;
    U64 requested_views = 0, adopted_views = 0, requested_cohort_draws = 0, indirect_draws = 0, fallback_draws = 0;
    U64 command_count = 0, gpu_receipts_ready = 0, cpu_packets_reclaimed = 0;
    std::array<GroupPublication::Image, 3> pixels;
    std::array<std::vector<U64>, 3> query_results;
};
void selectFamily(MaterialGroupPublication& f, const Group& group)
{ f.flavor = group.flavor; f.type = group.type; }
void addQuery(Arm& arm, std::size_t offset)
{
    ALMetalWorldVisibilityQuery q;
    std::memcpy(q.modelview_projection, arm.model.getF32ptr(), 64);
    q.center[0] = .35f; q.center[1] = -.5f; q.center[2] = -.5f;
    q.half_extent[0] = q.half_extent[1] = q.half_extent[2] = .08f;
    q.batch_offset = offset; q.fan = 7; q.count_samples = 1;
    arm.queries.push_back(q);
}
// This is the fixture's explicit postSort/capture frontier, using actual
// production publication/compilation/capture bodies. It does not reproduce
// the viewer's octree traversal, camera policy or material dependency walker.
void capture(MaterialGroupPublication& f, Arm& arm, U32 sources_per_group, Sample& sample)
{
    auto& capture = *arm.pipeline.mMetalWorldSimpleCapture;
    Inputs::LLCullResult cull;
    struct Pending { Group* group; U32 raw_begin; };
    std::vector<Pending> pending;
    arm.queries.clear(); addQuery(arm, 0);
    U32 logical = 0; bool first = true;
    const auto current = [&](Group& group)
    { return Inputs::ALSceneGroupPublicationNative::currentMaterial(group.state, arm.region.snapshot(),
        group.flavor, Inputs::sNativeSceneSources, Inputs::gFrameCount); };
    for (U32 family = 1; family <= 2; ++family)
    {
        const auto type = family == 1 ? Inputs::LLRenderPass::PASS_MATERIAL : Inputs::LLRenderPass::PASS_GLTF_PBR;
        cull.pushDrawInfo(type, arm.holes[family - 1]); ++logical; // A required raw gap in each pass.
        for (auto& owner : arm.groups)
        {
            auto& group = *owner; if (group.family != family) continue;
            if (first) { addQuery(arm, logical + sources_per_group / 2); first = false; }
            auto packet = current(group);
            if (packet)
            {
                group.compilation.publicationCompleted();
                requireWork(cull.pushNativeMaterialRange(group.type, packet, sources_per_group, arm.region.snapshot()), "Warm cohort cull admission failed");
            }
            else
            {
                const auto raw_begin = U32(cull.endRenderMap(group.type) - cull.beginRenderMap(group.type));
                for (auto* source : group.sources) cull.pushDrawInfo(group.type, source);
                pending.push_back({&group, raw_begin});
            }
            logical += sources_per_group; addQuery(arm, logical);
        }
    }
    LLVertexBuffer::flushBuffers();
    const auto frame = arm.budget.snapshot().frame;
    struct Cold
    {
        Group* group; U32 raw_begin, type;
        std::shared_ptr<ALNativePublication::State> publisher;
        ALNativeCohortCompilationAdmission* admission;
        Inputs::ALScenePublicationNative::WorldFlavor flavor;
    };
    std::vector<Cold> cold;
    for (auto& candidate : pending)
    {
        auto& admission = candidate.group->compilation; admission.notePending(frame);
        if (!admission.lastConsideredFrame() || frame > *admission.lastConsideredFrame())
            cold.push_back({candidate.group, candidate.raw_begin, candidate.group->type,
                candidate.group->state, &admission, candidate.group->flavor});
    }
    std::sort(cold.begin(), cold.end(), [](const auto& a, const auto& b)
    { return a.admission->priority(a.publisher->identity()) < b.admission->priority(b.publisher->identity()); });
    for (const auto& candidate : cold)
    {
        auto& group = *candidate.group;
        auto packet = current(group);
        if (packet) group.compilation.publicationCompleted();
        else packet = Inputs::ALSceneGroupCompilationNative::compile(*candidate.admission, candidate.publisher, arm.budget,
                group.sources.size(), group.family, [&]
                { return Inputs::ALSceneGroupPublicationNative::sealMaterial(candidate.publisher, group.dependencies, group.layout,
                    arm.model, arm.region.snapshot(), group.sources, candidate.flavor, Inputs::sNativeSceneSources,
                    Inputs::sNativeSceneSerial, Inputs::gFrameCount); }, true);
        if (packet)
            requireWork(cull.replaceNativeMaterialRange(candidate.type, packet, candidate.raw_begin, group.sources, arm.region.snapshot()),
                        "Same-view cold cohort replacement failed");
    }
    {
        CullScope scope(cull);
        for (U32 family = 1; family <= 2; ++family)
        {
            const auto at = std::find_if(arm.groups.begin(), arm.groups.end(), [family](const auto& g) { return g->family == family; });
            selectFamily(f, **at); capture.material_normal_map = f.flavor.normal_map;
            capture.material_specular_map = f.flavor.specular_map; capture.material_alpha_mode = f.flavor.material_alpha_mode;
            capture.alpha_mask = f.flavor.alpha_mask; capture.pbr = family == 2; f.capture(arm.pipeline);
        }
    }
    requireWork(capture.complete && capture.logicalDrawCount() == logical && capture.flushDrawOrder(), "Actual capture lost logical membership");
    sample.raw_draws += capture.batches.size(); sample.cohort_groups += capture.source_cohorts.size();
    sample.cohort_draws += capture.source_cohort_draw_count;
    sample.publication_debt_groups += arm.groups.size() - capture.source_cohorts.size();
}
Sample measure(MaterialGroupPublication& f, Arm& arm, U32 sources_per_group, U32 views, U32 condition, bool early, U32 frame)
{
    MaterialScope material_scope(arm);
    Sample sample;
    auto& capture_owner = *arm.pipeline.mMetalWorldSimpleCapture;
    struct Cancel { Arm& arm; ~Cancel() { for (auto& r : arm.renderers) r->cancelCommandBatch(); } } cancel{arm};
    const auto before_early_main = arm.renderers[0]->worldEarlyVisibilityStats();
    const auto before_early_probe = arm.renderers[1]->worldEarlyVisibilityStats();
    const auto before_group = Inputs::ALSceneGroupPublicationNative::statistics;
    const auto before_compile = Inputs::ALSceneGroupCompilationNative::statistics;
    const auto before_bytes = arm.data->stats().published_bytes;
    const auto before_commands = ALMetalDevice::committedCommandBufferCount();
    std::array<Receipt, 3> a_receipts, b_receipts;
    std::array<Ticket, 3> tickets;
    std::array<std::shared_ptr<ALMetalVisibilityResultBatch>, 3> results;
    std::array<std::unique_ptr<ALMetalRenderTarget>, 3> targets;
    const auto started = now(); arm.data->beginFrame(frame);
    if (condition == 0)
    {
        for (auto& g : arm.groups)
        { g->state->invalidate(); g->layout.reset(); g->dependencies.reset(); for (auto* source : g->sources) source->state->invalidate(); }
        for (auto* hole : arm.holes) hole->state->invalidate();
    }
    if (condition == 2)
    {
        const LLColor4U tint = frame % 2 ? LLColor4U(0,255,0,255) : LLColor4U(0,0,255,255);
        const std::vector<LLColor4U> colors(768, tint); U32 ordinal = 0;
        for (auto& group : arm.groups) for (auto* source : group->sources)
            if (ordinal++ % 16 == 0)
            { source->mVertexBuffer->setColorData(colors.data()); source->mVertexBuffer->unmapBuffer(); }
    }
    for (U32 view_index = 0; view_index < views; ++view_index)
    {
        arm.model.setIdentity(); arm.model.getF32ptr()[12] = .01f * view_index;
        requireWork(arm.region.update(arm.model, LLVector3(.01f * view_index, 0, 0)), "Region publication failed");
        capture_owner.clearDraws(); capture_owner.complete = true;
        selectFamily(f, *arm.groups.front()); f.configure(capture_owner, false, false);
        capture_owner.world_resources = std::make_shared<Inputs::WorldResources>();
        if (early)
        {
            sample.pre_cull_batches += capture_owner.batches.size();
            const std::array regions{arm.region.snapshot()};
            auto candidates = Registry::snapshot(regions);
            sample.candidates += candidates.size();
            std::vector<ALMetalWorldSourceCohortView> early_views;
            std::size_t logical = 0;
            for (const auto& candidate : candidates)
            { early_views.push_back(earlyView(candidate, logical)); logical += candidate.cohort->size(); }
            sample.candidate_rows += logical;
            sample.snapshot_copy_bytes += candidates.capacity() * sizeof(Registry::Candidate) + early_views.capacity() * sizeof(ALMetalWorldSourceCohortView);
            if (!early_views.empty())
            {
                ++sample.prefix_attempts;
                if (!arm.visibility)
                {
                    const auto factory_start = now();
                    arm.visibility = ALMetalWorldVisibility::create(*f.device, &f.error);
                    sample.first_visibility_creation_ns += now() - factory_start;
                }
                auto frozen = arm.visibility ? arm.visibility->makeView({}, early_views, logical, &f.error) : nullptr;
                auto command = frozen ? f.device->makeCommandBuffer("Actual early classification A before CPU discovery", &f.error) : nullptr;
                if (command) tickets[view_index] = ALMetalWorldVisibility::encodeEarly(*command, *frozen, &f.error, arm.data.get());
                if (tickets[view_index])
                {
                    a_receipts[view_index] = command->completionToken();
                    requireWork(command->commit(&f.error), "Early prefix native commit failed"); ++sample.prefix_commits;
                }
                else { ++sample.prefix_denied; f.error.clear(); }
            }
            // The candidates, View and all temporary CPU producer aliases end
            // here, before the first current view cull-map/LLDrawInfo discovery.
        }
        capture(f, arm, sources_per_group, sample);
        auto scene = f.scene(capture_owner); scene.visibility_queries = arm.queries.data(); scene.visibility_query_count = arm.queries.size();
        scene.material_argument_buffers = scene.source_command_banks = scene.source_direct_command_banks = scene.cpu_cohort_visibility = false;
        scene.early_visibility = tickets[view_index];
        if (a_receipts[view_index] && a_receipts[view_index]->isComplete()) ++sample.prefix_ready_at_consumer;
        auto inputs = ALMetalWorldViewInputs::adopt(scene, std::move(capture_owner.batches), std::move(capture_owner.source_references),
            std::move(capture_owner.draw_order), std::move(capture_owner.source_cohorts), std::move(arm.queries),
            std::move(capture_owner.cpu_sources), capture_owner.world_resources, frame, &f.error);
        requireWork(bool(inputs), "Late view adoption failed"); scene = {};
        auto& renderer = *arm.renderers[view_index ? 1 : 0];
        requireWork(renderer.beginCommandBatch("Actual early classification late native B", &f.error), "Late command admission failed");
        targets[view_index] = renderer.renderOwnedInputs(*inputs, &f.error, &results[view_index]);
        requireWork(bool(targets[view_index]) && bool(results[view_index]), "Actual late native render failed");
        b_receipts[view_index] = renderer.commandBatchCompletion();
        requireWork(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, capture_owner.batches, capture_owner.source_references,
            capture_owner.draw_order, capture_owner.source_cohorts, arm.queries), "Actual late CPU packet did not retire");
        ++sample.cpu_packets_reclaimed; capture_owner.clearDraws();
        capture_owner.world_resources = std::make_shared<Inputs::WorldResources>();
        requireWork(renderer.commitCommandBatch(&f.error), "Late native commit failed");
    }
    const auto encoded = now();
    for (U32 view = 0; view < views; ++view)
    {
        if (a_receipts[view]) wait(a_receipts[view]); wait(b_receipts[view]);
        sample.query_results[view].resize(results[view]->resultCount());
        for (std::size_t q = 0; q < sample.query_results[view].size(); ++q)
            requireWork(results[view]->wait(q, sample.query_results[view][q], &f.error), "Native query settlement failed");
        results[view].reset(); tickets[view].reset();
    }
    gGL.frameFences().release(); const auto settled = now();
    sample.owner_wall_ms = double(encoded - started) / 1e6; sample.settled_wall_ms = double(settled - started) / 1e6;
    sample.command_count = ALMetalDevice::committedCommandBufferCount() - before_commands;
    const auto after_compile = Inputs::ALSceneGroupCompilationNative::statistics;
    sample.compile_admitted = after_compile.admitted - before_compile.admitted;
    sample.compile_denied = after_compile.denied - before_compile.denied;
    sample.stability_deferred = after_compile.stability_deferred - before_compile.stability_deferred;
    sample.group_builds = Inputs::ALSceneGroupPublicationNative::statistics.builds - before_group.builds;
    sample.native_publication_bytes = arm.data->stats().published_bytes - before_bytes;
    const auto count_delta = [&](const ALMetalWorldEarlyVisibilityStats& before, const ALMetalWorldEarlyVisibilityStats& after)
    { sample.requested_views += after.requested_views - before.requested_views; sample.adopted_views += after.adopted_views - before.adopted_views;
      sample.requested_cohort_draws += after.requested_cohort_draws - before.requested_cohort_draws;
      sample.indirect_draws += after.indirect_draws - before.indirect_draws; sample.fallback_draws += after.fallback_draws - before.fallback_draws; };
    count_delta(before_early_main, arm.renderers[0]->worldEarlyVisibilityStats());
    count_delta(before_early_probe, arm.renderers[1]->worldEarlyVisibilityStats());
    CHECK(sample.command_count == views + sample.prefix_commits); CHECK(sample.pre_cull_batches == 0);
    CHECK(sample.cpu_packets_reclaimed == views);
    CHECK(sample.requested_views == sample.prefix_commits); CHECK(sample.adopted_views <= sample.requested_views);
    CHECK(sample.raw_draws >= 2 * views); CHECK(sample.indirect_draws <= sample.cohort_draws);
    CHECK(sample.requested_cohort_draws <= sample.cohort_draws);
    CHECK(sample.requested_cohort_draws == sample.indirect_draws + sample.fallback_draws);
    if (!early) { CHECK(sample.prefix_commits == 0); CHECK(sample.requested_views == 0); }
    double first_gpu = std::numeric_limits<double>::max(), last_gpu = 0;
    const auto timing = [&](const Receipt& receipt, double& sum)
    {
        if (!receipt) return;
        ALMetalCommandGPUTime time; REQUIRE(receipt->gpuTime(time) == ALMetalCommandTimingStatus::ready);
        sum += double(time.elapsed_nanoseconds) / 1e6; ++sample.gpu_receipts_ready;
        first_gpu = std::min(first_gpu, time.start_seconds); last_gpu = std::max(last_gpu, time.end_seconds);
    };
    for (U32 view = 0; view < views; ++view)
    {
        timing(a_receipts[view], sample.gpu_A_ms); timing(b_receipts[view], sample.gpu_B_ms);
        sample.pixels[view] = f.read(*targets[view]); targets[view].reset();
        REQUIRE(sample.query_results[view].size() == arm.groups.size() + 2);
        for (auto value : sample.query_results[view]) CHECK(value > 0);
    }
    CHECK(sample.gpu_receipts_ready == sample.command_count);
    sample.gpu_envelope_ms = (last_gpu - first_gpu) * 1000.;
    return sample;
}
double median(std::vector<double> values)
{ std::sort(values.begin(), values.end()); return (values[3] + values[4]) * .5; }
void initialize(MaterialGroupPublication& f, Arm& arm, U32 group_count, U32 sources_per_group,
    bool outside_half, bool early, const std::shared_ptr<const ALMetalShaderLibrary>& library)
{
    REQUIRE(arm.region.update(arm.model, LLVector3(0,0,0)));
    for (U32 index = 0; index < group_count; ++index)
    {
        auto group = std::make_unique<Group>(); group->family = 1 + index % 2;
        group->type = group->family == 1 ? Inputs::LLRenderPass::PASS_MATERIAL : Inputs::LLRenderPass::PASS_GLTF_PBR;
        group->flavor.family = static_cast<Inputs::ALScenePublicationNative::WorldFamily>(group->family);
        group->flavor.native_materials = true; group->flavor.native_resources = false; group->flavor.cohort_bounds = early;
        selectFamily(f, *group);
        for (U32 source = 0; source < sources_per_group; ++source)
        {
            auto& info = f.materialDraw(source % 4 != 3); info.mModelMatrix = &arm.model;
            // One-source groups alternate outside in both families; larger
            // groups contain both inside/outside source rows.
            const bool outside = outside_half && (sources_per_group == 1 ? (index / 2) % 2 : source % 2);
            visibilityCostMesh(info, 768, outside);
            LLStrider<LLVector3> positions; REQUIRE(info.mVertexBuffer->getVertexStrider(positions));
            for (U32 vertex = 0; vertex < 768; ++vertex)
            { positions[vertex].mV[0] *= .75f; positions[vertex].mV[1] *= .75f; }
            info.mVertexBuffer->unmapBuffer(); REQUIRE(info.mCount == 768);
            group->sources.push_back(&info);
        }
        arm.groups.push_back(std::move(group));
    }
    for (U32 family = 1; family <= 2; ++family)
    {
        const auto at = std::find_if(arm.groups.begin(), arm.groups.end(), [family](const auto& g) { return g->family == family; });
        selectFamily(f, **at); auto& info = f.materialDraw(false); info.mModelMatrix = &arm.model;
        visibilityCostMesh(info, 768, false); arm.holes[family - 1] = &info;
    }
    arm.material_cache = std::make_unique<ALMetalWorldMaterialDataCache>(*f.device);
    arm.data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(arm.data);
    arm.data->setSourcePublicationAdmission([budget = &arm.budget, pending = std::optional<ALNativeFrameBudget::Frame>{}]
        (const ALMetalWorldSourcePublicationWork& work) mutable -> std::shared_ptr<const void>
    {
        if (!pending) pending = budget->snapshot().frame;
        auto decision = budget->tryAcquire(ALNativeFrameBudget::Work::scene_publication, work.chargedBytes(), *pending);
        if (decision) pending.reset(); return decision.credit;
    });
    for (auto& renderer : arm.renderers)
    {
        renderer = ALMetalWorldGBufferRenderer::create(*f.device, 1, library, &f.error);
        INFO(f.error); REQUIRE(renderer); renderer->setFrameData(arm.data);
    }
}
}
}

TEST_CASE("Actual early static cohort classification compares selected stable direct capture through native settlement",
          "[almetal][.native-early-visibility-cost]")
{
    using namespace ActualEarlyVisibility;
    const auto enabled = [](const char* name)
    { const auto* value = std::getenv(name); return value && value[0] && value[0] != '0'; };
    if (!enabled("ALCHEMY_EARLY_VISIBILITY_COST")) SKIP("Set ALCHEMY_EARLY_VISIBILITY_COST=1 for the actual-owner frontier");
#ifndef NDEBUG
    SKIP("Actual early visibility timing requires optimized code");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    for (const auto shape : {std::array<U32,2>{8,1}, {64,4}})
    for (const bool half_outside : {false, true}) for (const U32 condition : {0u,1u,2u}) for (const U32 views : {1u,3u})
    {
        MaterialGroupPublication f(1); RegistryScope registry;
        Inputs::gSavedSettings.native_groups = true;
        Inputs::gSavedSettings.native_resources = Inputs::gSavedSettings.native_visibility = false;
        auto library_data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(library_data);
        auto library = library_data->builtinLibrary(&f.error); REQUIRE(library);
        std::array<Arm,2> arms;
        for (U32 mode = 0; mode < 2; ++mode) initialize(f, arms[mode], shape[0], shape[1], half_outside, mode != 0, library);
        std::array<std::vector<double>,2> owner_times, settled_times;
        std::cout << "ACTUAL_EARLY_SCOPE sources=" << shape[0] * shape[1]
            << " groups=" << shape[0] << " sources_per_group=" << shape[1] << " views=" << views
            << " condition=" << condition << " actual_outside_sources=" << (half_outside ? shape[0] * shape[1] / 2 : 0)
            << " logical_sources_per_view=" << shape[0] * shape[1] + 2
            << " indices_per_source=768 unique_indices=1 raw_holes_per_view=2 selected_stability=1 baseline_bounds=0 candidate_bounds=1"
               " outside_fraction_applies_to_candidate_groups_raw_holes_inside=1"
               " registry_enabled_both_arms_baseline_unqualified_O1_hook=1 snapshot_before_cull_and_capture=1 classify_only=1 CPU_bindings_eliminated=0"
               " no_forced_prefix_wait=1 synthetic_idle_budget=1 views_share_same_outer_frame_arena=1 cold_means_unstable_every_frame=1"
               " initial_source_bounds_and_gpu_publications_timed=1 no_untimed_preseal=1 loading_debt_reported=1"
               " candidate_visibility_factory_lazy_timed=1 common_world_renderer_factory_excluded_both_arms=1"
               " includes=dirty_updates_publication_snapshot_A_encode_commit_cull_capture_B_adopt_encode_commit_query_settlement_CPUreclaim_GPUcompletion_owner_ticket_release"
               " excludes=assets_DrawInfo_creation_live_octree_camera_policy_dependencywalker_target_wrapper_retirement_parity_readback"
               " target_wrappers_retained_until_parity_outside_timer=1 physical_slot_retirement_latency_unmeasured=1 workload_is_not_SL_geometry_histogram=1 FPS_claim=0\n";
        for (U32 sample_index = 0; sample_index < 10; ++sample_index)
        {
            REQUIRE(std::chrono::steady_clock::now() < deadline);
            ++Inputs::gFrameCount;
            for (auto& arm : arms)
            { ALNativeFrameBudget::FrameInputs frame; frame.frame = Inputs::gFrameCount; arm.budget.beginFrame(frame); }
            std::array<Sample,2> paired;
            for (U32 order = 0; order < 2; ++order)
            {
                const U32 mode = order ^ (sample_index & 1u);
                auto value = measure(f, arms[mode], shape[1], views, condition, mode != 0, Inputs::gFrameCount);
                if (sample_index >= 2) { owner_times[mode].push_back(value.owner_wall_ms); settled_times[mode].push_back(value.settled_wall_ms); }
                std::cout << "ACTUAL_EARLY_SAMPLE sources=" << shape[0] * shape[1] << " groups=" << shape[0]
                    << " outside=" << half_outside << " condition=" << condition << " views=" << views
                    << " sample=" << sample_index << " mode=" << mode << " phase=" << (sample_index < 2 ? "startup" : "measured")
                    << " owner_wall_ms=" << value.owner_wall_ms << " settled_wall_ms=" << value.settled_wall_ms
                    << " gpu_A_ms=" << value.gpu_A_ms << " gpu_B_ms=" << value.gpu_B_ms << " gpu_envelope_ms=" << value.gpu_envelope_ms
                    << " candidates=" << value.candidates << " candidate_rows=" << value.candidate_rows << " snapshot_copy_bytes=" << value.snapshot_copy_bytes
                    << " first_visibility_creation_ns=" << value.first_visibility_creation_ns
                    << " pre_cull_CPU_batches=" << value.pre_cull_batches << " raw_draws=" << value.raw_draws << " cohort_draws=" << value.cohort_draws
                    << " publication_debt_groups=" << value.publication_debt_groups << " native_publication_bytes=" << value.native_publication_bytes
                    << " prefix_attempts=" << value.prefix_attempts << " prefix_commits=" << value.prefix_commits << " prefix_denied=" << value.prefix_denied
                    << " prefix_ready_at_consumer=" << value.prefix_ready_at_consumer << " requested_views=" << value.requested_views << " adopted_views=" << value.adopted_views
                    << " requested_cohort_draws=" << value.requested_cohort_draws << " indirect_submissions=" << value.indirect_draws << " direct_fallback_draws=" << value.fallback_draws
                    << " group_builds=" << value.group_builds << " compile_admitted=" << value.compile_admitted << " compile_denied=" << value.compile_denied
                    << " stability_deferred=" << value.stability_deferred << " commands=" << value.command_count
                    << " expected_commands=" << views + value.prefix_commits << " gpu_receipts_ready=" << value.gpu_receipts_ready
                    << " cpu_packets_reclaimed=" << value.cpu_packets_reclaimed << '\n';
                paired[mode] = std::move(value);
            }
            CHECK(paired[0].pixels == paired[1].pixels); CHECK(paired[0].query_results == paired[1].query_results);
        }
        std::cout << "ACTUAL_EARLY_SUMMARY sources=" << shape[0] * shape[1] << " outside=" << half_outside
            << " condition=" << condition << " views=" << views << " samples=8"
            << " direct_owner_wall_median_ms=" << median(owner_times[0]) << " early_owner_wall_median_ms=" << median(owner_times[1])
            << " direct_settled_wall_median_ms=" << median(settled_times[0]) << " early_settled_wall_median_ms=" << median(settled_times[1]) << '\n';
    }
}
