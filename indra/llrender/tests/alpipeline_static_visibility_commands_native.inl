/**
 * @file alpipeline_static_visibility_commands_native.inl
 * @brief Actual retained static producers through same-command visible banks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace
{
namespace ActualStaticVisibilityCommands
{
using ActualEarlyVisibility::Arm;
using ActualEarlyVisibility::MaterialScope;
using ActualEarlyVisibility::initialize;
using ActualEarlyVisibility::now;
using ActualEarlyVisibility::requireWork;
using ActualEarlyVisibility::selectFamily;
using ActualEarlyVisibility::wait;
struct Sample : ActualEarlyVisibility::Sample
{
    double owner_thread_cpu_ms = 0, gpu_ms = 0;
    U64 bank_requested = 0, bank_draws = 0, fallback = 0, execute_calls = 0;
    U64 bank_requests = 0, bank_hits = 0, bank_builds = 0, bank_evictions = 0;
    U64 bank_admission_denials = 0, bank_argument_bytes = 0, bank_binding_bytes = 0;
    U64 direct_builds = 0, direct_hits = 0, direct_encoded_calls = 0;
    U64 visibility_dispatches = 0, visibility_rows = 0, visibility_qualified_rows = 0;
    U64 page_requests = 0, page_hits = 0, page_builds = 0, page_failures = 0;
    U64 page_admission_denials = 0, page_row_bytes = 0, page_mapping_bytes = 0;
    U64 page_copy_bytes = 0, page_bank_copy_bytes = 0, page_arena_bytes = 0;
    U64 direct_policy_bytes = 0, direct_slot_allocations = 0, direct_slot_hits = 0, direct_view_bytes = 0;
    U64 declaration_calls = 0, declared_resources = 0;
    U64 scalar_common_builds = 0, scalar_recipe_reads = 0, scalar_common_reuses = 0;
    U64 source_getters = 0, dependency_walks = 0, image_getters = 0;
    U64 publication_debt_after_settle = 0;
};
U64 threadCPU()
{
    timespec value{};
    requireWork(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0, "Owner thread clock failed");
    return U64(value.tv_sec) * 1'000'000'000ULL + value.tv_nsec;
}
void initializeArm(MaterialGroupPublication& f, Arm& arm, U32 groups, U32 sources,
                   bool outside_half, U32 mode, const std::shared_ptr<const ALMetalShaderLibrary>& library)
{
    initialize(f, arm, groups, sources, outside_half, mode == 2, library);
    if (outside_half && groups == 2 && sources == 1)
    {
        // The inherited fixture alternates whole one-source groups in pairs.
        // With only two groups place the PBR row outside explicitly: one row
        // per family remains an intentionally declared whole-group stress.
        auto& info = *arm.groups[1]->sources[0];
        visibilityCostMesh(info, 768, true);
        LLStrider<LLVector3> positions; REQUIRE(info.mVertexBuffer->getVertexStrider(positions));
        for (U32 vertex = 0; vertex < 768; ++vertex)
        { positions[vertex].mV[0] *= .75f; positions[vertex].mV[1] *= .75f; }
        info.mVertexBuffer->unmapBuffer();
    }
}
Sample measure(MaterialGroupPublication& f, Arm& arm, U32 sources_per_group, U32 condition, U32 mode, U32 frame)
{
    MaterialScope material_scope(arm);
    Sample sample;
    auto& renderer = *arm.renderers[0];
    auto& capture_owner = *arm.pipeline.mMetalWorldSimpleCapture;
    struct Cancel { ALMetalWorldGBufferRenderer& renderer; ~Cancel() { renderer.cancelCommandBatch(); } } cancel{renderer};
    const auto before_draw = renderer.worldSourceCommandStats();
    const auto before_page = renderer.worldSourceStats().pages;
    const auto before_cohort = renderer.worldCohortStats();
    const auto before_group = Inputs::ALSceneGroupPublicationNative::statistics;
    const auto before_compile = Inputs::ALSceneGroupCompilationNative::statistics;
    const auto before_bytes = arm.data->stats().published_bytes;
    const auto before_reads = f.reads();
    const auto before_commands = ALMetalDevice::committedCommandBufferCount();
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    std::unique_ptr<ALMetalRenderTarget> target;
    const auto started = now(), started_cpu = threadCPU();
    arm.data->beginFrame(frame);
    if (condition == 0)
    {
        for (auto& group : arm.groups)
        { group->state->invalidate(); group->layout.reset(); group->dependencies.reset();
          for (auto* source : group->sources) source->state->invalidate(); }
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
    arm.model.setIdentity();
    // A current late view changes even while immutable source geometry stays
    // warm. No view/readback from an earlier command can classify this one.
    arm.model.getF32ptr()[12] = .005f * (frame % 3);
    requireWork(arm.region.update(arm.model, LLVector3(.005f * (frame % 3),0,0)), "Region publication failed");
    capture_owner.clearDraws(); capture_owner.complete = true;
    selectFamily(f, *arm.groups.front()); f.configure(capture_owner, false, false);
    capture_owner.world_resources = std::make_shared<Inputs::WorldResources>();
    ActualEarlyVisibility::capture(f, arm, sources_per_group, sample);
    auto scene = f.scene(capture_owner);
    scene.visibility_queries = arm.queries.data(); scene.visibility_query_count = arm.queries.size();
    scene.material_argument_buffers = scene.cpu_cohort_visibility = false;
    scene.source_command_banks = scene.source_direct_command_banks = mode != 0;
    scene.source_gpu_visibility = mode == 2;
    scene.source_command_minimum_run = 2;
    auto inputs = ALMetalWorldViewInputs::adopt(scene, std::move(capture_owner.batches),
        std::move(capture_owner.source_references), std::move(capture_owner.draw_order),
        std::move(capture_owner.source_cohorts), std::move(arm.queries),
        std::move(capture_owner.cpu_sources), capture_owner.world_resources, frame, &f.error);
    requireWork(bool(inputs), "Actual static view adoption failed"); scene = {};
    requireWork(renderer.beginCommandBatch("Actual static direct/visible bank complete cost", &f.error), "Native command admission failed");
    target = renderer.renderOwnedInputs(*inputs, &f.error, &results);
    requireWork(bool(target) && bool(results), "Actual static native render failed");
    const auto receipt = renderer.commandBatchCompletion();
    requireWork(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, capture_owner.batches,
        capture_owner.source_references, capture_owner.draw_order, capture_owner.source_cohorts, arm.queries), "Actual static CPU packet did not retire");
    ++sample.cpu_packets_reclaimed; capture_owner.clearDraws();
    capture_owner.world_resources = std::make_shared<Inputs::WorldResources>();
    requireWork(renderer.commitCommandBatch(&f.error), "Actual static native commit failed");
    const auto encoded = now(), encoded_cpu = threadCPU();
    // This is final command/query settlement, never a visibility-prefix wait
    // or CPU decision about a GPU classification result before rendering.
    wait(receipt);
    sample.query_results[0].resize(results->resultCount());
    for (std::size_t q = 0; q < sample.query_results[0].size(); ++q)
        requireWork(results->wait(q, sample.query_results[0][q], &f.error), "Actual static query settlement failed");
    results.reset();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (arm.budget.snapshot().scene_publications_in_flight && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    sample.publication_debt_after_settle = arm.budget.snapshot().scene_publications_in_flight;
    requireWork(sample.publication_debt_after_settle == 0, "Source publication credit did not retire");
    gGL.frameFences().release(); const auto settled = now();
    sample.owner_wall_ms = double(encoded - started) / 1e6;
    sample.owner_thread_cpu_ms = double(encoded_cpu - started_cpu) / 1e6;
    sample.settled_wall_ms = double(settled - started) / 1e6;
    sample.command_count = ALMetalDevice::committedCommandBufferCount() - before_commands;
    const auto after_draw = renderer.worldSourceCommandStats();
    const auto after_page = renderer.worldSourceStats().pages;
    const auto after_cohort = renderer.worldCohortStats();
    const auto after_compile = Inputs::ALSceneGroupCompilationNative::statistics;
    const auto after_reads = f.reads();
    sample.group_builds = Inputs::ALSceneGroupPublicationNative::statistics.builds - before_group.builds;
    sample.compile_admitted = after_compile.admitted - before_compile.admitted;
    sample.compile_denied = after_compile.denied - before_compile.denied;
    sample.stability_deferred = after_compile.stability_deferred - before_compile.stability_deferred;
    sample.native_publication_bytes = arm.data->stats().published_bytes - before_bytes;
    sample.source_getters = after_reads[0] - before_reads[0];
    sample.dependency_walks = after_reads[1] - before_reads[1]; sample.image_getters = after_reads[2] - before_reads[2];
    sample.bank_requested = after_draw.requested_draws - before_draw.requested_draws;
    sample.bank_draws = after_draw.bank_draws - before_draw.bank_draws;
    sample.fallback = after_draw.fallback_draws - before_draw.fallback_draws;
    sample.execute_calls = after_draw.execute_calls - before_draw.execute_calls;
    const auto& a = after_draw.cache; const auto& b = before_draw.cache;
    sample.bank_requests = a.requests - b.requests; sample.bank_hits = a.hits - b.hits;
    sample.bank_builds = a.builds - b.builds; sample.bank_evictions = a.evictions - b.evictions;
    sample.bank_admission_denials = a.admission_denials - b.admission_denials;
    sample.bank_argument_bytes = a.charged_logical_argument_bytes - b.charged_logical_argument_bytes;
    sample.bank_binding_bytes = a.charged_logical_binding_bytes - b.charged_logical_binding_bytes;
    sample.direct_builds = a.direct_builds - b.direct_builds; sample.direct_hits = a.direct_hits - b.direct_hits;
    sample.direct_encoded_calls = a.direct_encoded_calls - b.direct_encoded_calls;
    sample.visibility_dispatches = a.static_visibility_dispatches - b.static_visibility_dispatches;
    sample.visibility_rows = a.static_visibility_rows - b.static_visibility_rows;
    sample.visibility_qualified_rows = a.static_visibility_qualified_rows - b.static_visibility_qualified_rows;
    sample.page_requests = after_page.cohort_requests - before_page.cohort_requests;
    sample.page_hits = after_page.cohort_hits - before_page.cohort_hits;
    sample.page_builds = after_page.cohort_builds - before_page.cohort_builds;
    sample.page_failures = after_page.cohort_failures - before_page.cohort_failures;
    sample.page_admission_denials = after_page.admission_denials - before_page.admission_denials;
    sample.page_row_bytes = after_page.encoded_row_bytes - before_page.encoded_row_bytes;
    sample.page_mapping_bytes = after_page.encoded_mapping_bytes - before_page.encoded_mapping_bytes;
    sample.page_copy_bytes = after_page.copied_page_bytes - before_page.copied_page_bytes;
    sample.page_bank_copy_bytes = after_page.copied_bank_bytes - before_page.copied_bank_bytes;
    sample.page_arena_bytes = after_page.arena_view_bytes - before_page.arena_view_bytes;
    sample.direct_policy_bytes = after_page.direct_policy_bytes - before_page.direct_policy_bytes;
    sample.direct_slot_allocations = after_page.direct_slot_allocations - before_page.direct_slot_allocations;
    sample.direct_slot_hits = after_page.direct_slot_hits - before_page.direct_slot_hits;
    sample.direct_view_bytes = after_page.direct_view_bytes - before_page.direct_view_bytes;
    sample.declaration_calls = after_page.cohort_declaration_calls - before_page.cohort_declaration_calls;
    sample.declared_resources = after_page.cohort_declared_resources - before_page.cohort_declared_resources;
    sample.scalar_common_builds = after_cohort.scalar_common_builds - before_cohort.scalar_common_builds;
    sample.scalar_recipe_reads = after_cohort.scalar_recipe_reads - before_cohort.scalar_recipe_reads;
    sample.scalar_common_reuses = after_cohort.scalar_common_reuses - before_cohort.scalar_common_reuses;
    // Catch checks, GPU timing queries and parity readback are outside both
    // wall boundaries. Native context/cache construction happened inside.
    ALMetalCommandGPUTime gpu;
    REQUIRE(receipt->gpuTime(gpu) == ALMetalCommandTimingStatus::ready);
    sample.gpu_ms = double(gpu.elapsed_nanoseconds) / 1e6; ++sample.gpu_receipts_ready;
    sample.pixels[0] = f.read(*target); target.reset();
    REQUIRE(sample.query_results[0].size() == arm.groups.size() + 2);
    // Inherited addQuery draws a separate always-inside proxy at each logical
    // boundary. These are not counts of the preceding source's geometry:
    // even the whole-outside one-row PBR stress retains this positive control.
    for (auto count : sample.query_results[0]) CHECK(count > 0);
    CHECK(std::any_of(sample.pixels[0].depth.begin(), sample.pixels[0].depth.end(), [](F32 d) { return d < 1.f; }));
    CHECK(sample.raw_draws + sample.cohort_draws == arm.groups.size() * sources_per_group + 2);
    CHECK(sample.raw_draws >= 2); CHECK(sample.command_count == 1);
    CHECK(sample.cpu_packets_reclaimed == 1); CHECK(sample.bank_draws + sample.fallback == sample.bank_requested);
    CHECK(sample.bank_draws <= sample.cohort_draws);
    if (!mode) { CHECK(sample.bank_requested == 0); CHECK(sample.visibility_dispatches == 0); }
    if (mode != 2) { CHECK(sample.visibility_dispatches == 0); CHECK(sample.visibility_rows == 0); CHECK(sample.visibility_qualified_rows == 0); }
    CHECK(sample.visibility_qualified_rows <= sample.visibility_rows);
    if (sample.bank_draws == sample.cohort_draws && sample.bank_draws)
    { CHECK(sample.scalar_recipe_reads == 0); CHECK(sample.scalar_common_builds == sample.raw_draws); }
    return sample;
}
void beginFrame(std::array<Arm, 3>& arms)
{
    ++Inputs::gFrameCount;
    for (auto& arm : arms)
    { ALNativeFrameBudget::FrameInputs frame; frame.frame = Inputs::gFrameCount; arm.budget.beginFrame(frame); }
}
void parity(const std::array<Sample, 3>& paired)
{
    for (U32 mode = 1; mode < 3; ++mode)
    { CHECK(paired[0].pixels[0] == paired[mode].pixels[0]); CHECK(paired[0].query_results[0] == paired[mode].query_results[0]); }
}
double median(std::vector<double> values)
{ std::sort(values.begin(), values.end()); return (values[3] + values[4]) * .5; }
void emit(const Sample& s, U32 groups, U32 sources, bool outside, U32 condition, U32 sample, U32 mode)
{
    std::cout << "STATIC_VISIBLE_SAMPLE groups=" << groups << " sources_per_group=" << sources
        << " candidate_rows=" << groups * sources << " rows_per_family=" << groups * sources / 2
        << " outside=" << outside << " condition=" << condition << " views=1 sample=" << sample << " mode=" << mode
        << " phase=" << (sample < 2 ? "startup" : "measured")
        << " owner_wall_ms=" << s.owner_wall_ms << " owner_thread_cpu_ms=" << s.owner_thread_cpu_ms
        << " settled_wall_ms=" << s.settled_wall_ms << " gpu_ms=" << s.gpu_ms
        << " raw_draws=" << s.raw_draws << " cohort_draws=" << s.cohort_draws << " cohort_groups=" << s.cohort_groups
        << " publication_debt_groups=" << s.publication_debt_groups << " publication_credit_debt_after_settle=" << s.publication_debt_after_settle
        << " group_builds=" << s.group_builds << " compile_admitted=" << s.compile_admitted
        << " compile_denied=" << s.compile_denied << " stability_deferred=" << s.stability_deferred
        << " native_publication_bytes=" << s.native_publication_bytes
        << " bank_requested=" << s.bank_requested << " bank_draws=" << s.bank_draws << " fallback=" << s.fallback
        << " execute_calls=" << s.execute_calls << " bank_requests=" << s.bank_requests
        << " bank_hits=" << s.bank_hits << " bank_builds=" << s.bank_builds << " bank_evictions=" << s.bank_evictions
        << " bank_admission_denials=" << s.bank_admission_denials << " bank_argument_bytes=" << s.bank_argument_bytes
        << " bank_binding_bytes=" << s.bank_binding_bytes << " direct_builds=" << s.direct_builds
        << " direct_hits=" << s.direct_hits << " direct_encoded_calls=" << s.direct_encoded_calls
        << " visibility_dispatches=" << s.visibility_dispatches << " visibility_rows=" << s.visibility_rows
        << " visibility_qualified_rows=" << s.visibility_qualified_rows
        << " page_requests=" << s.page_requests << " page_hits=" << s.page_hits << " page_builds=" << s.page_builds
        << " page_failures=" << s.page_failures << " page_admission_denials=" << s.page_admission_denials
        << " page_row_bytes=" << s.page_row_bytes << " page_mapping_bytes=" << s.page_mapping_bytes
        << " page_copy_bytes=" << s.page_copy_bytes << " page_bank_copy_bytes=" << s.page_bank_copy_bytes
        << " page_arena_bytes=" << s.page_arena_bytes << " direct_policy_bytes=" << s.direct_policy_bytes
        << " direct_slot_allocations=" << s.direct_slot_allocations << " direct_slot_hits=" << s.direct_slot_hits
        << " direct_view_bytes=" << s.direct_view_bytes << " declaration_calls=" << s.declaration_calls
        << " declared_resources=" << s.declared_resources << " scalar_common_builds=" << s.scalar_common_builds
        << " scalar_recipe_reads=" << s.scalar_recipe_reads << " scalar_common_reuses=" << s.scalar_common_reuses
        << " source_getters=" << s.source_getters << " dependency_walks=" << s.dependency_walks
        << " image_getters=" << s.image_getters << " commands=" << s.command_count
        << " gpu_receipts_ready=" << s.gpu_receipts_ready << " cpu_packets_reclaimed=" << s.cpu_packets_reclaimed << '\n';
}
}
}

TEST_CASE("Actual static visible commands preserve selected producer queries and current views through churn",
          "[almetal][pipeline-buffer-publication][static-visibility-command-producer]")
{
    using namespace ActualStaticVisibilityCommands;
    for (bool outside : {false, true})
    {
        MaterialGroupPublication f(1);
        Inputs::gSavedSettings.native_groups = true;
        Inputs::gSavedSettings.native_resources = Inputs::gSavedSettings.native_visibility = false;
        auto library_data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(library_data);
        auto library = library_data->builtinLibrary(&f.error); REQUIRE(library);
        std::array<Arm, 3> arms;
        for (U32 mode = 0; mode < 3; ++mode) initializeArm(f, arms[mode], 2, 4, outside, mode, library);
        bool full_count_warm = false, visible_warm = false;
        for (U32 step = 0; step < 6; ++step)
        {
            beginFrame(arms); std::array<Sample, 3> paired;
            const U32 condition = step < 3 ? 1 : step < 5 ? 2 : 1;
            for (U32 mode = 0; mode < 3; ++mode)
                paired[mode] = measure(f, arms[mode], 4, condition, mode, Inputs::gFrameCount);
            parity(paired);
            if (step == 0)
                for (const auto& s : paired) { CHECK(s.cohort_draws == 0); CHECK(s.stability_deferred == 2); }
            if (step == 2)
            {
                for (U32 mode = 1; mode < 3; ++mode)
                {
                    const auto& s = paired[mode];
                    CHECK(s.publication_debt_groups == 0); CHECK(s.bank_draws == 8); CHECK(s.fallback == 0);
                    CHECK(s.direct_hits > 0); CHECK(s.direct_builds == 0); CHECK(s.direct_encoded_calls == 0);
                    CHECK(s.direct_slot_allocations == 0); CHECK(s.direct_policy_bytes == 0);
                    CHECK(s.page_row_bytes == 0); CHECK(s.page_mapping_bytes == 0);
                    CHECK(s.scalar_recipe_reads == 0); CHECK(s.scalar_common_builds == 2);
                }
                full_count_warm = paired[1].bank_draws == 8;
                visible_warm = paired[2].bank_draws == 8 && paired[2].visibility_dispatches > 0 && paired[2].visibility_rows == 8;
                CHECK(paired[2].visibility_qualified_rows == 8);
            }
            if (step == 3)
                for (const auto& s : paired) { CHECK(s.raw_draws > 2); CHECK(s.stability_deferred > 0); }
        }
        CHECK(full_count_warm); CHECK(visible_warm);
    }
}

TEST_CASE("Actual one-row static passes keep native direct fallback under the production minimum",
          "[almetal][pipeline-buffer-publication][static-visibility-command-producer]")
{
    using namespace ActualStaticVisibilityCommands;
    MaterialGroupPublication f(1);
    Inputs::gSavedSettings.native_groups = true;
    Inputs::gSavedSettings.native_resources = Inputs::gSavedSettings.native_visibility = false;
    auto library_data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(library_data);
    auto library = library_data->builtinLibrary(&f.error); REQUIRE(library);
    std::array<Arm, 3> arms;
    for (U32 mode = 0; mode < 3; ++mode) initializeArm(f, arms[mode], 2, 1, true, mode, library);
    for (U32 step = 0; step < 3; ++step)
    {
        beginFrame(arms); std::array<Sample, 3> paired;
        for (U32 mode = 0; mode < 3; ++mode)
            paired[mode] = measure(f, arms[mode], 1, 1, mode, Inputs::gFrameCount);
        parity(paired);
        for (U32 mode = 1; mode < 3; ++mode)
        { CHECK(paired[mode].bank_draws == 0); CHECK(paired[mode].visibility_dispatches == 0); }
        if (step == 2)
            for (U32 mode = 1; mode < 3; ++mode)
            { CHECK(paired[mode].cohort_draws == 2); CHECK(paired[mode].fallback == 2); }
    }
}

TEST_CASE("Actual selected static direct and same-command visible banks compare complete native costs",
          "[almetal][.native-static-visibility-command-cost]")
{
    using namespace ActualStaticVisibilityCommands;
    const auto enabled = [](const char* name)
    { const auto* value = std::getenv(name); return value && value[0] && value[0] != '0'; };
    if (!enabled("ALCHEMY_STATIC_VISIBILITY_COMMAND_COST")) SKIP("Set ALCHEMY_STATIC_VISIBILITY_COMMAND_COST=1 for the actual-owner frontier");
#ifndef NDEBUG
    SKIP("Static visible command timing requires optimized code");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    for (const auto shape : {std::array<U32,2>{2,1}, {2,2}, {2,4}, {64,4}})
    for (bool outside : {false, true}) for (U32 condition : {0u,1u,2u})
    {
        MaterialGroupPublication f(1);
        Inputs::gSavedSettings.native_groups = true;
        Inputs::gSavedSettings.native_resources = Inputs::gSavedSettings.native_visibility = false;
        auto library_data = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(library_data);
        auto library = library_data->builtinLibrary(&f.error); REQUIRE(library);
        std::array<Arm, 3> arms;
        for (U32 mode = 0; mode < 3; ++mode) initializeArm(f, arms[mode], shape[0], shape[1], outside, mode, library);
        std::array<std::vector<double>, 3> owner_times, settled_times, gpu_times;
        std::cout << "STATIC_VISIBLE_SCOPE groups=" << shape[0] << " sources_per_group=" << shape[1]
            << " candidate_rows=" << shape[0] * shape[1] << " rows_per_family=" << shape[0] * shape[1] / 2
            << " outside=" << outside << " actual_outside_rows=" << (outside ? shape[0] * shape[1] / 2 : 0)
            << " condition=" << condition << " condition_name=" << (condition == 0 ? "cold_unstable_every_frame" : condition == 1 ? "requested_warm" : "churn_one_of_sixteen")
            << " views=1 mode0=selected_stable_direct_boundsOFF mode1=fullcount_DIRECTbank_boundsOFF"
               " mode2=samecommand_GPUvisible_DIRECTbank_boundsON samples=8 startup=2 production_minimum_run=2"
               " mixed_Legacy_PBR=1 raw_holes=2 raw_holes_inside=1 query_inside_first_group_and_after_each_group=1 separate_visible_proxy_queries=1"
               " indices_per_source=768 unique_indices=1 source_geometry_not_SL_histogram=1"
               " one_row_whole_group_outside_stress=" << (outside && shape[1] == 1)
            << " mixed_groups_preserve_inside_and_outside=" << (outside && shape[1] > 1)
            << " selected_stability=1 real_default16_publication_budget=1 no_quota_override=1 no_untimed_preseal=1"
               " current_view_changes=1 one_outer_frame_arena=1 CPU_cull_capture_still_occurs=1 no_visibility_CPU_readback=1"
               " no_forced_prefix_wait=1 no_early_job=1 synthetic_idle_budget_inputs=1 candidate_factory_debt_timed=1"
               " bank_draws_are_encoded_logical_commands=1 visibility_rows_are_encoded_not_visible=1"
               " includes=dirty_bounds_source_publication_cull_capture_adoption_bankprepare_compute_nativeencode_commit_query_GPUsettlement_CPUreclaim_creditretirement"
               " excludes=initial_assets_DrawInfo_live_octree_camera_dependencywalker_common_renderer_factory_parity_readback_targetwrapper_physical_slot_retirement_latency"
               " all_adverse_and_unsettled_rows_retained=1 FPS_claim=0\n";
        for (U32 sample = 0; sample < 10; ++sample)
        {
            REQUIRE(std::chrono::steady_clock::now() < deadline);
            beginFrame(arms); std::array<Sample, 3> paired;
            for (U32 order = 0; order < 3; ++order)
            {
                const U32 mode = (sample + order) % 3;
                auto value = measure(f, arms[mode], shape[1], condition, mode, Inputs::gFrameCount);
                emit(value, shape[0], shape[1], outside, condition, sample, mode);
                if (sample >= 2)
                { owner_times[mode].push_back(value.owner_wall_ms); settled_times[mode].push_back(value.settled_wall_ms); gpu_times[mode].push_back(value.gpu_ms); }
                paired[mode] = std::move(value);
            }
            parity(paired);
        }
        std::cout << "STATIC_VISIBLE_SUMMARY groups=" << shape[0] << " sources_per_group=" << shape[1]
            << " outside=" << outside << " condition=" << condition << " views=1 samples=8";
        for (U32 mode = 0; mode < 3; ++mode)
            std::cout << " mode" << mode << "_owner_median_ms=" << median(owner_times[mode])
                << " mode" << mode << "_settled_median_ms=" << median(settled_times[mode])
                << " mode" << mode << "_gpu_median_ms=" << median(gpu_times[mode]);
        std::cout << '\n';
    }
}
