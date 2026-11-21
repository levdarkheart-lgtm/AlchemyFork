/**
 * @file pipeline_compact_capture_native.inl
 * @brief Ordered raw/source capture storage shared with actual-owner fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include inside the native capture owner after its storage declarations.
 */
std::vector<ALMetalWorldSourceCohortView> source_cohorts;
std::size_t source_cohort_draw_count = 0;
U64 direct_cohort_appends = 0, direct_cohort_draws = 0;
std::size_t logicalDrawCount() const noexcept
{
    return batches.size() + source_references.size() + source_cohort_draw_count;
}
void clearDraws() noexcept
{
    batches.clear();
    source_references.clear();
    draw_order.clear();
    source_cohorts.clear();
    source_cohort_draw_count = 0;
    raw_draw_watermark = 0;
    cpu_sources.reset();
    compact_gbuffer_active = false;
    clearAnimatedSources();
}
void retainCpu(std::shared_ptr<const void> owner)
{
    if (!cpu_sources) cpu_sources = std::make_shared<WorldCpuLease>();
    cpu_sources->retain(std::move(owner));
}
bool appendCohort(std::shared_ptr<const ALMetalWorldSourceCohort> cohort,
                  std::shared_ptr<const void> cpu_owner, std::shared_ptr<const void> native_owner,
                  const ALMetalWorldViewMatrices& view) noexcept
try
{
    const auto count = cohort ? cohort->size() : 0;
    const auto logical = logicalDrawCount();
    if (!count || !cohort->matchesOwners(cpu_owner, native_owner) ||
        logical > std::numeric_limits<U32>::max() || count > std::numeric_limits<U32>::max() - logical)
    { clearDraws();return false; }
    source_cohorts.push_back({logical, batches.size() + source_references.size(),
        std::move(cohort), std::move(cpu_owner), std::move(native_owner), view});
    source_cohort_draw_count += count;
    ++direct_cohort_appends;
    direct_cohort_draws += count;
    return true;
}
catch (const std::bad_alloc&) { clearDraws();return false; }
bool flushRawDrawPrefix() noexcept
try
{
    if (raw_draw_watermark > batches.size() ||
        (!batches.empty() && !alMetalWorldDrawIndexFits(batches.size() - 1)))
    { clearDraws(); return false; }
    for (; raw_draw_watermark < batches.size(); ++raw_draw_watermark)
        draw_order.push_back(static_cast<ALMetalWorldDrawIndex>(raw_draw_watermark));
    return true;
}
catch (const std::bad_alloc&)
{
    clearDraws();
    return false;
}
bool flushDrawOrder() noexcept
{
    // The ordinary raw-only path keeps its existing contiguous representation.
    return source_references.empty() || flushRawDrawPrefix();
}
bool appendReference(const ALMetalWorldSimpleBatch& source,
                     const ALMetalWorldViewMatrices& view) noexcept
try
{
    if (!alMetalWorldDrawIndexFits(source_references.size()) ||
        logicalDrawCount() >= std::numeric_limits<U32>::max() || !flushRawDrawPrefix())
    { clearDraws(); return false; }
    const auto index = static_cast<ALMetalWorldDrawIndex>(source_references.size());
    source_references.push_back({&source, view});
    draw_order.push_back(alMetalWorldDrawReferenceBit | index);
    return true;
}
catch (const std::bad_alloc&)
{
    clearDraws();
    return false;
}
