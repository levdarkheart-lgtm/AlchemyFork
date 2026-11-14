/**
 * @file pipeline_compact_capture_native.inl
 * @brief Ordered raw/source capture storage shared with actual-owner fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include inside the native capture owner after its storage declarations.
 */
std::size_t logicalDrawCount() const noexcept
{
    return batches.size() + source_references.size();
}
void clearDraws() noexcept
{
    batches.clear();
    source_references.clear();
    draw_order.clear();
    raw_draw_watermark = 0;
    cpu_sources.reset();
    compact_gbuffer_active = false;
}
void retainCpu(std::shared_ptr<const void> owner)
{
    if (!cpu_sources) cpu_sources = std::make_shared<WorldResourceLease>();
    cpu_sources->retain(std::move(owner));
}
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
