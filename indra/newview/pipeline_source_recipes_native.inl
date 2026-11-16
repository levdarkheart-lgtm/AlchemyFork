/**
 * @file pipeline_source_recipes_native.inl
 * @brief Bounded CPU producer recipes, independent of native residency.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the shared group CPU residency owner.
 */
namespace ALScenePublicationNative
{
struct RecipeImage
{
    std::weak_ptr<const ALMetalTexture> allocation;
    const ALMetalTexture* address = nullptr;
    U64 bytes = 0;
};
struct WorldRecipe
{
    U64 key = 0;
    ALSceneGroupPublicationNative::StaticRestoreTuple values;
    std::size_t weight_offset = 0;
    bool pbr_glow = false, impostor_fullbright = false, impostor_shiny = false, impostor_emboss = false;
    std::vector<RecipeImage> images;
    std::weak_ptr<const ALMetalWorldMaterialSource> material;
    std::weak_ptr<const ALMetalWorldResourceSource> resources;
    std::weak_ptr<const ALMetalWorldCohortSource> cohort;

    WorldRecipe(U64 flavor, const ALMetalWorldSimpleBatch& source)
        : key(flavor), values(source), weight_offset(source.weight_offset), pbr_glow(source.pbr_glow),
          impostor_fullbright(source.impostor_fullbright), impostor_shiny(source.impostor_shiny),
          impostor_emboss(source.impostor_emboss), material(source.native_material_source),
          resources(source.native_resource_source) {}
    U64 ownedBytes() const noexcept
    {
        return sizeof(*this) + values.ownedBytes() - sizeof(values) +
            U64(images.capacity()) * sizeof(RecipeImage);
    }
    void expand(ALMetalWorldSimpleBatch& value) const
    {
        values.expand(value);value.weight_offset = weight_offset;value.pbr_glow = pbr_glow;
        value.impostor_fullbright = impostor_fullbright;value.impostor_shiny = impostor_shiny;
        value.impostor_emboss = impostor_emboss;
    }
};
struct ShadowRecipe
{
    U32 type = 0;
    ALMetalWorldSpotShadowCoverage coverage = ALMetalWorldSpotShadowCoverage::opaque;
    std::shared_ptr<const ALMetalShadowSourceRecipe> source;
};
struct SourceRecipe
{
    U64 identity = 0, revision = 0;
    std::weak_ptr<const ALMetalBuffer> vertices, indices;
    const ALMetalBuffer* vertex_address = nullptr;
    const ALMetalBuffer* index_address = nullptr;
    U64 vertex_bytes = 0, index_bytes = 0;
    ALMetalWorldConservativeBounds bounds;
    bool bounds_attempted = false;
    std::vector<std::shared_ptr<const WorldRecipe>> worlds;
    std::vector<ShadowRecipe> shadows;
    U64 ownedBytes() const noexcept
    {
        U64 bytes = sizeof(*this) + U64(worlds.capacity()) * sizeof(worlds[0]) +
            U64(shadows.capacity()) * sizeof(shadows[0]);
        for (const auto& world : worlds) bytes += world->ownedBytes();
        for (const auto& shadow : shadows) bytes += shadow.source->ownedBytes();
        return bytes;
    }
    std::size_t rows() const noexcept { return worlds.size() + shadows.size(); }
};
struct RecipeStats
{
    U64 publications = 0, world_restores = 0, shadow_restores = 0;
    U64 expired_backing = 0, unsupported = 0, failures = 0, stale = 0;
};
inline RecipeStats recipe_statistics;
// Optional CPU-only construction/recovery seam. A producer mutation from this
// callback is deliberately caught by the same final revision checks as a real
// reentrant resource publication; a failed recipe never removes required work.
inline void (*recipeAllocationForTesting)(ALNativePublication::State&) = nullptr;

inline std::shared_ptr<const SourceRecipe> recipeFor(const LLDrawInfo& info, U32 frame)
{
    const auto& metadata = info.mMetalShadowSource;
    if (!metadata || !metadata->native_scene_recipe) return {};
    auto recipe = std::static_pointer_cast<const SourceRecipe>(metadata->native_scene_recipe);
    const auto& state = info.metalSourcePublication();
    if (recipe->identity != state->identity() || recipe->revision != state->revision())
    {
        ALSceneGroupPublicationNative::cpu_layouts.forgetRecipe(recipe->identity);
        ++recipe_statistics.stale;return {};
    }
    if (!ALSceneGroupPublicationNative::cpu_layouts.touchRecipe(recipe->identity, recipe, frame))
    { metadata->native_scene_recipe.reset();return {}; }
    return recipe;
}
inline bool recipeWorldSupported(const ALMetalWorldSimpleBatch& value) noexcept
{
    // These are source prototypes, before per-view adoption. Do not retain a
    // special pass, palette, raw pose or borrowed CPU record in a recipe.
    // The complete small-run check found the cheap SIMPLE reconstruction
    // faster than weak-tuple restoration. Keep its original direct factory;
    // material families and certified shadow recipes share this CPU owner.
    return (value.material_slot_count || value.pbr_slot_count) &&
        value.vertices && value.indices && value.index_count &&
        value.texture_count <= value.max_diffuse_textures &&
        value.material_slot_count <= value.max_material_slots && value.pbr_slot_count <= value.max_material_slots &&
        !value.terrain_pass && !value.impostor_texture_owner && !value.terrain_texture_owner &&
        !value.body_palette_record && !value.skin_palette_record && !value.skin_palette &&
        !value.skin_palette_float_count && !value.skin_joint_count && !value.normal_texture &&
        !value.cloth_offset && !value.avatar_rigid && !value.avatar_skinning && !value.avatar_cloth &&
        !value.avatar_alpha && !value.impostor && !value.terrain && !value.water_exclusion &&
        !value.water_haze && !value.water_surface && !value.atmospheric_haze &&
        !value.pre_water_alpha && !value.post_water_alpha && !value.post_water_depth_only &&
        !value.post_water_glow && !value.legacy_glow;
}
inline void rememberRecipe(const LLDrawInfo& info, const std::shared_ptr<Payload>& value,
                           U32 frame, bool churn) noexcept
try
{
    const auto& state = info.metalSourcePublication();
    if (state->revision() != value->producer_revision || !info.mMetalShadowSource) return;
    if (!ALSceneGroupPublicationNative::cpu_layouts.recipesEnabled()) return;
    auto previous = recipeFor(info, frame);
    // First successful native-resident publication pays no new recipe
    // allocation. Admit CPU facts only after real denial/retirement or when
    // extending an already useful recipe with another consumer variant.
    if (!previous && !churn) return;
    const auto has_world = [&](U64 key)
    { return previous && std::any_of(previous->worlds.begin(), previous->worlds.end(),
        [&](const auto& world) { return world->key == key; }); };
    const auto has_shadow = [&](const ShadowVariant& shadow)
    { return previous && std::any_of(previous->shadows.begin(), previous->shadows.end(),
        [&](const auto& entry) { return entry.type == shadow.type && entry.coverage == shadow.coverage; }); };
    bool eligible = !value->shadows.empty();
    for (const auto& world : value->worlds) eligible = eligible || recipeWorldSupported(world.prototype);
    if (!eligible) return;
    bool changed = !previous || previous->bounds_attempted != value->bounds_attempted;
    for (const auto& world : value->worlds)
        if (recipeWorldSupported(world.prototype)) changed = changed || !has_world(world.key);
    for (const auto& shadow : value->shadows) changed = changed || !has_shadow(shadow);
    if (!changed) return;
    std::size_t worlds = previous ? previous->worlds.size() : 0;
    std::size_t shadows = previous ? previous->shadows.size() : 0;
    for (const auto& world : value->worlds)
        if (recipeWorldSupported(world.prototype) && !has_world(world.key)) ++worlds;
    for (const auto& shadow : value->shadows) if (!has_shadow(shadow)) ++shadows;
    const U64 minimum_bytes = sizeof(SourceRecipe) + U64(worlds) * (sizeof(WorldRecipe) + sizeof(std::shared_ptr<const WorldRecipe>)) +
        U64(shadows) * (sizeof(ShadowRecipe) + sizeof(ALMetalShadowSourceRecipe));
    if (!ALSceneGroupPublicationNative::cpu_layouts.mayRetainRecipe(state->identity(),worlds+shadows,minimum_bytes,frame)) return;
    if (recipeAllocationForTesting) recipeAllocationForTesting(*state);
    auto recipe = previous ? std::make_shared<SourceRecipe>(*previous) : std::make_shared<SourceRecipe>();
    recipe->identity = state->identity();recipe->revision = value->producer_revision;
    recipe->vertices = value->vertices;recipe->indices = value->indices;
    recipe->vertex_address = value->vertices.get();recipe->index_address = value->indices.get();
    recipe->vertex_bytes = value->vertices->length();recipe->index_bytes = value->indices->length();
    recipe->bounds = value->bounds;recipe->bounds_attempted = value->bounds_attempted;
    for (const auto& variant : value->worlds)
    {
        if (has_world(variant.key)) continue;
        if (!recipeWorldSupported(variant.prototype)) { ++recipe_statistics.unsupported;continue; }
        auto world = std::make_shared<WorldRecipe>(variant.key, variant.prototype);
        world->cohort = variant.cohort_source;
        const auto image = [&](const ALMetalTexture* pointer)
        {
            if (!pointer) return true;
            if (std::any_of(world->images.begin(), world->images.end(),
                [&](const auto& existing) { return existing.address == pointer; })) return true;
            // The completed producer already computed this exact native
            // wrapper's descriptor cost. Preserve it instead of rescanning
            // mip/block descriptors during every restoration.
            const auto found = std::find_if(value->resources.begin(), value->resources.end(),
                [&](const auto& resource) { return resource.allocation.get() == pointer; });
            if (found == value->resources.end() || static_cast<const void*>(pointer) == value->vertices.get() ||
                static_cast<const void*>(pointer) == value->indices.get()) return false;
            const std::shared_ptr<const ALMetalTexture> typed(found->allocation, pointer);
            world->images.push_back({typed, pointer, found->bytes});return true;
        };
        const auto& prototype = variant.prototype;
        bool complete = true;
        for (U32 slot = 0; slot < prototype.texture_count; ++slot)
            complete = complete && prototype.diffuse_textures[slot] && image(prototype.diffuse_textures[slot]);
        for (U32 slot = 0; slot < prototype.material_slot_count; ++slot)
            complete = complete && image(prototype.materialSlots()[slot].normal_texture) && image(prototype.materialSlots()[slot].specular_texture);
        for (U32 slot = 0; slot < prototype.pbr_slot_count; ++slot)
            complete = complete && image(prototype.pbrSlots()[slot].normal_texture) &&
                image(prototype.pbrSlots()[slot].orm_texture) && image(prototype.pbrSlots()[slot].emissive_texture);
        if (!complete) { ++recipe_statistics.unsupported;continue; }
        recipe->worlds.push_back(std::move(world));
    }
    for (const auto& shadow : value->shadows)
        if (!has_shadow(shadow))
            if (auto source = ALMetalShadowSourceRecipe::create(*shadow.record))
                recipe->shadows.push_back({shadow.type, shadow.coverage, std::move(source)});
    if (!recipe->rows() || state->identity() != recipe->identity || state->revision() != recipe->revision)
    { ++recipe_statistics.stale;return; }
    if (ALSceneGroupPublicationNative::cpu_layouts.retainRecipe(info.mMetalShadowSource,
        recipe->identity, recipe, recipe->ownedBytes(), recipe->rows(), frame))
        ++recipe_statistics.publications;
}
catch (const std::bad_alloc&) { ++recipe_statistics.failures; }

inline std::shared_ptr<Payload> recipePayload(const LLDrawInfo& info,
    const SourceRecipe& recipe, U64& serial)
{
    auto vertices = recipe.vertices.lock(), indices = recipe.indices.lock();
    if (!vertices || !indices || vertices.get() != recipe.vertex_address || indices.get() != recipe.index_address)
    { ++recipe_statistics.expired_backing;return {}; }
    const auto& state = info.metalSourcePublication();
    if (state->identity() != recipe.identity || state->revision() != recipe.revision) return {};
    // Preserve other live variants when extending a current publication, but
    // never call a mutable producer or resurrect any old native generation.
    std::shared_ptr<const Payload> current;
    if (state->publication()) current = std::static_pointer_cast<const Payload>(state->publication());
    else if (info.mMetalShadowSource && info.mMetalShadowSource->native_scene_revision == recipe.revision)
        current = std::static_pointer_cast<const Payload>(info.mMetalShadowSource->native_scene_transient.lock());
    auto value = current && current->producer_revision == recipe.revision
        ? std::make_shared<Payload>(*current) : std::make_shared<Payload>();
    value->serial = ++serial;value->producer_revision = recipe.revision;
    value->vertices = std::move(vertices);value->indices = std::move(indices);
    value->bounds = recipe.bounds;value->bounds_attempted = recipe.bounds_attempted;
    account(*value, value->vertices, recipe.vertex_bytes);account(*value, value->indices, recipe.index_bytes);
    return value;
}
inline WorldLease restoreWorldRecipe(const LLDrawInfo& info, const WorldFlavor& flavor,
    ALNativeSourceWorkingSet& working_set, U64& serial, U32 frame)
try
{
    if (flavor.family == WorldFamily::simple || flavor.animated_source) return {};
    auto recipe = recipeFor(info, frame);if (!recipe) return {};
    const auto found = std::find_if(recipe->worlds.begin(), recipe->worlds.end(),
        [&](const auto& world) { return world->key == flavor.key(); });
    if (found == recipe->worlds.end()) return {};
    const auto& source = **found;
    const auto& state = info.metalSourcePublication();
    if (recipeAllocationForTesting) recipeAllocationForTesting(*state);
    auto value = recipePayload(info, *recipe, serial);if (!value) return {};
    // Pin all typed textures before expanding raw CPU slot values. Expired
    // or recycled addresses cannot manufacture ownership from the recipe.
    for (const auto& image : source.images)
    {
        auto texture = image.allocation.lock();
        if (!texture || texture.get() != image.address) { ++recipe_statistics.expired_backing;return {}; }
        account(*value, std::move(texture), image.bytes);
    }
    std::erase_if(value->worlds, [&](const auto& world) { return world.key == flavor.key(); });
    auto& variant = value->worlds.emplace_back();variant.key = flavor.key();source.expand(variant.prototype);
    auto& prototype = variant.prototype;
    if (flavor.native_materials && flavor.family != WorldFamily::simple)
    {
        prototype.native_material_source = source.material.lock();
        if (!prototype.native_material_source || !prototype.native_material_source->matches(prototype))
        {
            auto* device = LLImageGL::getMetalDevice();if (!device) return {};
            if (!material_data_cache || !material_data_cache->belongsTo(*device))
                material_data_cache = std::make_unique<ALMetalWorldMaterialDataCache>(*device);
            if (!ALMetalWorldMaterialSource::publish(*device, prototype, *material_data_cache)) return {};
        }
        const auto retain = [&](const std::shared_ptr<const ALMetalBuffer>& buffer)
        { if (buffer) account(*value, buffer, buffer->length()); };
        retain(prototype.native_material_source->materialLease());
        retain(prototype.native_material_source->pbrLease(false));retain(prototype.native_material_source->pbrLease(true));
    }
    if (!flavor.native_materials) variant.cpu_slots_frozen = prototype.freezeCpuSlots();
    if (flavor.visibility_bounds || flavor.cohort_bounds) qualifyBounds(info, prototype, *value);
    if (flavor.native_resources)
    {
        prototype.native_resource_source = source.resources.lock();
        if (!prototype.native_resource_source || !prototype.native_resource_source->matches(prototype))
            publishResourceSource(*value, prototype);
    }
    if (flavor.cohort_source)
    {
        variant.cohort_source = source.cohort.lock();
        sealCohortSource(*value, variant, recipe->identity, recipe->revision);
        if (!variant.cohort_source) return {};
    }
    if (state->identity() != recipe->identity || state->revision() != recipe->revision ||
        !publish(info, value, working_set, frame)) return {};
    ++recipe_statistics.world_restores;
    return {std::move(value), &prototype, nullptr, true};
}
catch (const std::bad_alloc&) { ++recipe_statistics.failures;return {}; }
inline ShadowLease restoreShadowRecipe(const LLDrawInfo& info, U32 type,
    ALMetalWorldSpotShadowCoverage coverage, ALNativeSourceWorkingSet& working_set,
    U64& serial, U32 frame)
try
{
    auto recipe = recipeFor(info, frame);if (!recipe) return {};
    const auto found = std::find_if(recipe->shadows.begin(), recipe->shadows.end(),
        [&](const auto& shadow) { return shadow.type == type && shadow.coverage == coverage; });
    if (found == recipe->shadows.end()) return {};
    const auto& state = info.metalSourcePublication();
    if (recipeAllocationForTesting) recipeAllocationForTesting(*state);
    auto value = recipePayload(info, *recipe, serial);if (!value) return {};
    auto record = found->source->restore(value->serial);
    if (!record) { ++recipe_statistics.expired_backing;return {}; }
    account(*value, record->vertexLease(), found->source->vertexBytes());
    account(*value, record->indexLease(), found->source->indexBytes());
    for (std::size_t slot = 0; slot < record->textureLeases().size(); ++slot)
        account(*value, record->textureLeases()[slot], found->source->textureBytes(slot));
    std::erase_if(value->shadows, [&](const auto& shadow) { return shadow.type == type && shadow.coverage == coverage; });
    value->shadows.push_back({type, coverage, std::move(record)});
    if (state->identity() != recipe->identity || state->revision() != recipe->revision ||
        !publish(info, value, working_set, frame)) return {};
    ++recipe_statistics.shadow_restores;
    const auto* result = &value->shadows.back().record;
    return {std::move(value), result, nullptr, true};
}
catch (const std::bad_alloc&) { ++recipe_statistics.failures;return {}; }
}
