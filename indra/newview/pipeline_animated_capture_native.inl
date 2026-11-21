/**
 * @file pipeline_animated_capture_native.inl
 * @brief Owner-qualified CPU animated-source capture shared with fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include inside Capture before pipeline_compact_capture_native.inl.
 */
bool animated_sources_active = false;
bool animated_direct_stability = false;
// Borrowed owner policy, never copied to a flavor, publication or GPU view.
ALNativeFrameBudget* animated_compilation_budget = nullptr;
struct AnimatedSourceCaptureStats
{
    // Owner capture decisions, distinct from actual accepted GPU execution.
    std::uint64_t enabled_views=0,requested_draws=0,captured_draws=0;
    std::uint64_t unsupported_skips=0,source_unavailable=0,owner_proof_skips=0,pose_unavailable=0;
    std::uint64_t capacity_fallbacks=0,allocation_fallbacks=0;
} animated_source_stats;
void clearAnimatedSources() noexcept
{animated_sources_active=animated_direct_stability=false;animated_compilation_budget=nullptr;}
void beginAnimatedSources(bool enabled,ALNativeFrameBudget* budget=nullptr,
                          bool stabilize_direct=false) noexcept
{
    animated_sources_active=enabled;
    animated_direct_stability=enabled&&stabilize_direct;
    animated_compilation_budget=enabled?budget:nullptr;
    if(enabled)++animated_source_stats.enabled_views;
}
bool tryCaptureAnimatedSource(const LLDrawInfo& info,
    const ALScenePublicationNative::WorldLease& source,
    const ALScenePublicationNative::WorldFlavor& flavor,
    const ALMetalWorldViewMatrices& view,
    const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& palette,bool eligible) noexcept
try
{
    if(!animated_sources_active)return false;
    ++animated_source_stats.requested_draws;
    if(!eligible || !flavor.hasAnimatedConsumer() || info.mTextureMatrix || !info.mAvatar || !info.mSkinInfo)
    {++animated_source_stats.unsupported_skips;return false;}
    if(!palette || !palette->jointCount() || palette->jointCount()>110 ||
       palette->size()!=4+std::size_t(palette->jointCount())*12)
    {++animated_source_stats.pose_unavailable;return false;}
    const auto* certificate=ALScenePublicationNative::animatedSourceFor(source,flavor);
    if(!certificate || !*certificate)
    {++animated_source_stats.source_unavailable;return false;}
    const auto& producer=info.metalSourcePublication();
    const auto& key=(*certificate)->key();
    if(!source.owner->native_tuple || producer->identity()!=key.identity ||
       producer->revision()!=key.revision || source.owner->producer_revision!=key.revision)
    {++animated_source_stats.owner_proof_skips;return false;}
    // All fallible storage/pins precede ordered-reference insertion. Optional
    // allocation failure must leave every existing raw/ref draw untouched and
    // let this same source/current palette take the required ordinary path.
    if(raw_draw_watermark>batches.size() || !alMetalWorldDrawIndexFits(source_references.size()) ||
       logicalDrawCount()>=std::numeric_limits<U32>::max())
    {++animated_source_stats.capacity_fallbacks;return false;}
    const auto raw_count=batches.size()-raw_draw_watermark;
    if(draw_order.size()>draw_order.max_size()-1 || raw_count>draw_order.max_size()-draw_order.size()-1 ||
       source_references.size()==source_references.max_size() ||
       (!batches.empty() && !alMetalWorldDrawIndexFits(batches.size()-1)))
    {++animated_source_stats.capacity_fallbacks;return false;}
    source_references.reserve(source_references.size()+1);
    draw_order.reserve(draw_order.size()+raw_count+1);
    retainCpu(source.owner);
    retainCpu(palette);
    ALMetalWorldSourceReference reference;
    reference.source=&(*certificate)->prototype();reference.view=view;
    reference.animated_source=certificate->get();reference.animated_palette=palette.get();
    for(;raw_draw_watermark<batches.size();++raw_draw_watermark)
        draw_order.push_back(static_cast<ALMetalWorldDrawIndex>(raw_draw_watermark));
    const auto index=static_cast<ALMetalWorldDrawIndex>(source_references.size());
    source_references.push_back(reference);
    draw_order.push_back(alMetalWorldDrawReferenceBit|index);
    ++animated_source_stats.captured_draws;
    return true;
}
catch(const std::bad_alloc&){++animated_source_stats.allocation_fallbacks;return false;}
