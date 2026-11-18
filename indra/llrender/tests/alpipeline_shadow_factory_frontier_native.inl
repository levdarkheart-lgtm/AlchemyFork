/** Verbatim production Scene::shadow factory; qualification pins its extracted body. */
template<typename BuildPrototype>
ShadowLease shadow(const LLDrawInfo& info, U32 type,
                   ALMetalWorldSpotShadowCoverage coverage,
                   ALNativeSourceWorkingSet& working_set, U64& serial, U32 frame,
                   BuildPrototype&& build_prototype)
{
    try
    {
        // PASS_ALPHA's sweep policy is legacy_blend; its existing GLTF
        // producer emits the PBR-specific blend family under that policy.
        if (coverage == ALMetalWorldSpotShadowCoverage::legacy_blend &&
            (info.mGLTFMaterial.get() || !info.mGLTFMaterialList.empty()))
            coverage = ALMetalWorldSpotShadowCoverage::pbr_blend;
        info.observeMetalSourceDependencies();
        if (auto value = current(info))
        {
            if (const auto* record = findShadow(*value, type, coverage))
            {
                working_set.touch(info.metalSourcePublication()->identity(), value->serial, frame);
                return {std::move(value), record, nullptr, true};
            }
            ++source_miss_statistics.shadow_misses[
                static_cast<std::size_t>(FactoryMissStage::variant_missing)];
        }
        else ++source_miss_statistics.shadow_misses[
            static_cast<std::size_t>(FactoryMissStage::current_payload_missing)];
        if (auto restored = restoreShadowRecipe(info, type, coverage, working_set, serial, frame)) return restored;
        if (!geometry(info)) return {{}, nullptr, "Native shadow geometry is unpublished or invalid"};
        ALMetalWorldSpotShadowBatch prototype;
        if (!build_prototype(prototype)) return {{}, nullptr, "Native shadow prototype is incomplete"};
        if (prototype.coverage != coverage || prototype.terrain || prototype.avatar ||
            prototype.texture_count > ALMetalWorldSpotShadowBatch::max_textures)
            return {{}, nullptr, "Native shadow prototype family/coverage is invalid"};
        // No live face transform/current pose is part of stable source data.
        // The native record clears pose ownership; per-view UV is copied into
        // the compact instance by the caller under the original pass policy.
        std::memcpy(prototype.texture_matrix, alMetalShadowIdentity.data(), sizeof(prototype.texture_matrix));
        info.observeMetalSourceDependencies();
        auto value = extend(info, serial);
        if (!value) return {{}, nullptr, "Native shadow geometry lease is missing"};
        std::vector<std::shared_ptr<const ALMetalTexture>> textures;
        textures.reserve(prototype.texture_count);
        for (std::size_t slot = 0; slot < prototype.texture_count; ++slot)
        {
            auto lease = matchImage(info, static_cast<const ALMetalWorldSpotShadowBatch&>(prototype).coverageData().textures[slot]);
            if (!lease) return {{}, nullptr, "Native shadow texture lease is missing"};
            account(*value, lease, textureBytes(*lease));
            // Keep every authored slot, including repeated white/default maps.
            textures.push_back(std::move(lease));
        }
        auto record = ALMetalShadowSourceRecord::create(value->serial, prototype,
            value->vertices, value->indices, std::move(textures));
        if (!record) return {{}, nullptr, "Native shadow source certification failed"};
        value->shadows.push_back({type, coverage, std::move(record)});
        if (!publish(info, value, working_set, frame))
            return {{}, nullptr, "Native shadow producer changed while sealing"};
        const auto* result = &value->shadows.back().record;
        return {std::move(value), result};
    }
    catch (const std::bad_alloc&)
    {
        return {{}, nullptr, "Native shadow publication allocation failed"};
    }
}
