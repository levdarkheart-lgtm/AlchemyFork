/**
 * @file pipeline_source_rows_capture_native.inl
 * @brief Owner-qualified optional source-row provenance shared with fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Include inside Capture before pipeline_compact_capture_native.inl.
 */
bool source_rows_active = false;
std::shared_ptr<ALMetalWorldSourceView> source_rows;
struct SourceRowsCaptureStats
{
    // Cumulative producer observations, never GPU executions. Appended
    // candidates can later be discarded by an optional metadata failure;
    // renderer requested/bound counters report the separate consumer boundary.
    std::uint64_t enabled_views = 0, registered_models = 0;
    std::uint64_t requested_ordinary_rows = 0, emitted_candidates = 0;
    std::uint64_t unsupported_skips = 0, owner_proof_skips = 0, optional_metadata_failures = 0;
} source_rows_stats;
// This map exists only on the capture owner. No pointer from it is copied to
// ALMetalWorldSourceView, workers, a native page or command completion.
std::unordered_map<const LLMatrix4a*, ALNativeRegionModel::Snapshot> source_model_pointers;
struct SourceRegionKey
{
    std::uint64_t identity = 0, revision = 0;
    ALMetalWorldSourceRegion view;
    bool operator==(const SourceRegionKey& other) const noexcept
    {
        return identity == other.identity && revision == other.revision &&
            std::memcmp(&view, &other.view, sizeof(view)) == 0;
    }
};
struct SourceRegionHash
{
    std::size_t operator()(const SourceRegionKey& key) const noexcept
    {
        std::size_t hash = static_cast<std::size_t>(key.identity ^ (key.revision * 0x9e3779b97f4a7c15ULL));
        const auto* bytes = reinterpret_cast<const unsigned char*>(&key.view);
        for (std::size_t at = 0; at < sizeof(key.view); ++at) hash = (hash ^ bytes[at]) * 1099511628211ULL;
        return hash;
    }
};
std::unordered_map<SourceRegionKey, std::uint32_t, SourceRegionHash> source_region_rows;

void clearSourceRows() noexcept
{
    source_rows_active = false;
    source_rows.reset();
    source_model_pointers.clear();
    source_region_rows.clear();
}
void beginSourceRows(bool enabled) noexcept
{
    clearSourceRows();
    source_rows_active = enabled;
    if (enabled) ++source_rows_stats.enabled_views;
}
void failSourceRowsMetadata() noexcept
{
    ++source_rows_stats.optional_metadata_failures;
    clearSourceRows();
}
void registerSourceModel(const LLMatrix4a* model, const ALNativeRegionModel::Snapshot& snapshot) noexcept
try
{
    if (!source_rows_active || !model) return;
    if (!snapshot || std::memcmp(model->getF32ptr(), snapshot.model.data(), sizeof(snapshot.model)) != 0)
    {
        // A replacement observation can withdraw a previously valid proof in
        // this same capture. Previously copied rows stay intact; future draws
        // must not inherit the old pointer's availability or model epoch.
        source_model_pointers.erase(model);
        return;
    }
    const auto found = source_model_pointers.find(model);
    if (found != source_model_pointers.end()) found->second = snapshot;
    else
    {
        if (source_model_pointers.size() >= alMetalWorldSourceMaximumRows)
        { failSourceRowsMetadata(); return; }
        source_model_pointers.emplace(model, snapshot);
        ++source_rows_stats.registered_models;
    }
}
catch (const std::bad_alloc&) { failSourceRowsMetadata(); }

void captureSourceRow(const LLDrawInfo& info,
                      const ALScenePublicationNative::WorldLease& source,
                      const ALScenePublicationNative::WorldFlavor& flavor,
                      const ALMetalWorldViewMatrices& view, bool eligible) noexcept
try
{
    using Family = ALScenePublicationNative::WorldFamily;
    if (!source_rows_active || !eligible) return;
    ++source_rows_stats.requested_ordinary_rows;
    if (!source || !source.owner->native_tuple ||
        (flavor.family != Family::legacy && flavor.family != Family::pbr) ||
        flavor.rigged || flavor.pbr_glow || flavor.impostor_fullbright ||
        flavor.impostor_shiny || flavor.impostor_emboss ||
        (flavor.family == Family::legacy && flavor.material_alpha_mode == 1) ||
        info.mTextureMatrix || info.mAvatar || info.mSkinInfo ||
        !source.prototype->native_resource_source ||
        !source.prototype->native_resource_source->matches(*source.prototype))
    { ++source_rows_stats.unsupported_skips; return; }
    const auto model = source_model_pointers.find(info.mModelMatrix);
    if (model == source_model_pointers.end() || !model->second ||
        std::memcmp(info.mModelMatrix->getF32ptr(), model->second.model.data(), sizeof(model->second.model)) != 0)
    { ++source_rows_stats.owner_proof_skips; return; }
    const auto& producer = info.metalSourcePublication();
    if (producer->revision() != source.owner->producer_revision)
    { ++source_rows_stats.owner_proof_skips; return; }
    if (source_rows && source_rows->candidates.size() >= alMetalWorldSourceMaximumRows)
    { failSourceRowsMetadata(); return; }
    if (!source_rows) source_rows = std::make_shared<ALMetalWorldSourceView>();
    SourceRegionKey key;
    key.identity = model->second.identity;
    key.revision = model->second.revision;
    std::memcpy(key.view.modelview_projection, view.modelview_projection, sizeof(key.view.modelview_projection));
    std::memcpy(key.view.modelview, view.modelview, sizeof(key.view.modelview));
    std::memcpy(key.view.normal_matrix, view.normal_matrix, sizeof(key.view.normal_matrix));
    std::uint32_t region = 0;
    const auto found = source_region_rows.find(key);
    if (found != source_region_rows.end()) region = found->second;
    else
    {
        if (source_rows->regions.size() >= alMetalWorldSourceMaximumRows)
        { failSourceRowsMetadata(); return; }
        region = static_cast<std::uint32_t>(source_rows->regions.size());
        source_region_rows.emplace(key, region);
        source_rows->regions.push_back(key.view);
    }
    source_rows->candidates.push_back({
        {producer->identity(), flavor.key(), source.owner->producer_revision}, logicalDrawCount(), region});
    ++source_rows_stats.emitted_candidates;
}
catch (const std::bad_alloc&) { failSourceRowsMetadata(); }
