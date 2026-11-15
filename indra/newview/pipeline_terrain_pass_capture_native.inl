/**
 * @file pipeline_terrain_pass_capture_native.inl
 * @brief One owner-local immutable terrain pass publication.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included at namespace scope by pipeline.cpp and lldrawpoolterrain.cpp.
 */
#pragma once
// Prerequisites are the caller's actual world, fetched-input, surface-map,
// camera and region types. Keeping this actual body separate also permits the
// existing actual-source fixture to supply only explicit owner frontiers.

// Lives on one actual pool traversal's stack. No region/viewer pointer enters
// the native record; exact producer/view snapshots only qualify owner reuse.
struct ALMetalWorldTerrainPassCapture
{
    // M1 Max complete-path comparison: 1/2/4 patches added CPU setup,
    // while 16/64/256 improved CPU and settlement in both terrain families.
    // Small passes keep their existing raw native construction.
    static constexpr bool shouldShare(std::size_t patch_count) noexcept
    { return patch_count >= 16; }
    std::shared_ptr<const ALMetalWorldTerrainPass> publication;
    ALNativeRegionModel::Snapshot region;
    std::array<float,16> view{},projection{};
    bool pbr=false;
    bool current(const LLViewerRegion& owner,bool desired_pbr) const noexcept
    {
        const auto now=owner.nativeModelSnapshot();
        const auto& camera=LLViewerCamera::getCurrent();
        return publication && pbr==desired_pbr && now && now.identity==region.identity && now.revision==region.revision &&
            std::memcmp(view.data(),camera.getModelview().getF32ptr(),sizeof(view))==0 &&
            std::memcmp(projection.data(),camera.getProjection().getF32ptr(),sizeof(projection))==0;
    }
    void remember(const LLViewerRegion& owner,bool desired_pbr,std::shared_ptr<const ALMetalWorldTerrainPass> value) noexcept
    {
        publication=std::move(value);region=owner.nativeModelSnapshot();pbr=desired_pbr;
        const auto& camera=LLViewerCamera::getCurrent();
        std::memcpy(view.data(),camera.getModelview().getF32ptr(),sizeof(view));
        std::memcpy(projection.data(),camera.getProjection().getF32ptr(),sizeof(projection));
    }
    // The caller has already retained/validated these exact current wrappers.
    // Select only the texture roles consumed by the completed patch; the
    // record verifies pointer identity and owns separate typed native leases.
    static std::shared_ptr<const ALMetalWorldTerrainPass> seal(
        const ALMetalWorldSimpleBatch& patch,
        const ALTerrainSurfaceMaps& maps,const ALMetalTerrainFetchedBindings* fetched,
        LLImageGL* const* images,std::size_t image_count)
    try
    {
        std::vector<std::shared_ptr<const ALMetalTexture>> textures;
        textures.reserve(22);
        textures.push_back(maps.getMetalHeightTextureLease());
        textures.push_back(maps.getMetalCompositionTextureLease());
        const auto append=[&](const LLImageGL* image){if(image)textures.push_back(image->getMetalTextureLease());};
        if(fetched)
        {
            for(const auto* slots:{&fetched->base,&fetched->normal,&fetched->orm,&fetched->emissive})
                for(const auto& binding:*slots)append(binding.image.get());
            append(fetched->blend.image.get());append(fetched->overlay.image.get());
        }
        for(std::size_t index=0;index<image_count;++index)append(images[index]);
        return ALMetalWorldTerrainPass::create(patch,textures);
    }
    catch(const std::bad_alloc&){return {};}
};
