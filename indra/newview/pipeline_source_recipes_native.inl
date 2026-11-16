/**
 * @file pipeline_source_recipes_native.inl
 * @brief Bounded covered-source CPU recipes independent of native residency.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the shared group CPU residency owner.
 */
namespace ALScenePublicationNative
{
struct ShadowRecipe
{
    U32 type = 0;
    ALMetalWorldSpotShadowCoverage coverage = ALMetalWorldSpotShadowCoverage::opaque;
    std::shared_ptr<const ALMetalShadowSourceRecipe> source;
};
struct SourceRecipe
{
    U64 identity = 0, revision = 0;
    std::vector<ShadowRecipe> shadows;
    U64 ownedBytes() const noexcept
    {
        U64 bytes = sizeof(*this) + U64(shadows.capacity()) * sizeof(shadows[0]);
        for (const auto& shadow : shadows) bytes += shadow.source->ownedBytes();
        return bytes;
    }
    std::size_t rows() const noexcept { return shadows.size(); }
};
struct RecipeStats
{
    U64 publications = 0, shadow_restores = 0;
    U64 expired_backing = 0, failures = 0, stale = 0;
};
inline RecipeStats recipe_statistics;
// Optional CPU-only construction/recovery seam. A failed recipe never removes
// required work, and actual producer mutations always invalidate its revision.
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
inline void rememberRecipe(const LLDrawInfo& info, const std::shared_ptr<Payload>& value,
                           U32 frame, bool churn) noexcept
try
{
    const auto& state = info.metalSourcePublication();
    if (state->revision() != value->producer_revision || !info.mMetalShadowSource ||
        !ALSceneGroupPublicationNative::cpu_layouts.recipesEnabled()) return;
    auto previous = recipeFor(info, frame);
    // First resident publication stays allocation-free. Only the measured
    // covered families are selected; opaque and world material sources retain
    // their existing factory. Their full unqualified recipe proposal and
    // adverse measurements remain on the separate local proposal branch.
    if (!previous && !churn) return;
    const auto eligible = [](const ShadowVariant& shadow)
    { return shadow.coverage != ALMetalWorldSpotShadowCoverage::opaque && bool(shadow.record); };
    const auto remembered = [&](const ShadowVariant& shadow)
    { return previous && std::any_of(previous->shadows.begin(),previous->shadows.end(),
        [&](const auto& entry) { return entry.type == shadow.type && entry.coverage == shadow.coverage; }); };
    std::size_t added = 0;
    for (const auto& shadow : value->shadows) if (eligible(shadow) && !remembered(shadow)) ++added;
    if (!added) return;
    const auto rows = (previous ? previous->rows() : 0) + added;
    const U64 minimum_bytes = sizeof(SourceRecipe) + U64(rows) * (sizeof(ShadowRecipe) + sizeof(ALMetalShadowSourceRecipe));
    if (!ALSceneGroupPublicationNative::cpu_layouts.mayRetainRecipe(state->identity(),rows,minimum_bytes,frame)) return;
    if (recipeAllocationForTesting) recipeAllocationForTesting(*state);
    auto recipe = previous ? std::make_shared<SourceRecipe>(*previous) : std::make_shared<SourceRecipe>();
    recipe->identity = state->identity();recipe->revision = value->producer_revision;
    for (const auto& shadow : value->shadows)
        if (eligible(shadow) && !remembered(shadow))
            if (auto source = ALMetalShadowSourceRecipe::create(*shadow.record))
                recipe->shadows.push_back({shadow.type,shadow.coverage,std::move(source)});
    if (recipe->rows() == rows - added || state->identity() != recipe->identity || state->revision() != recipe->revision)
    { ++recipe_statistics.stale;return; }
    if (ALSceneGroupPublicationNative::cpu_layouts.retainRecipe(info.mMetalShadowSource,
        recipe->identity,recipe,recipe->ownedBytes(),recipe->rows(),frame)) ++recipe_statistics.publications;
}
catch (const std::bad_alloc&) { ++recipe_statistics.failures; }

inline ShadowLease restoreShadowRecipe(const LLDrawInfo& info, U32 type,
    ALMetalWorldSpotShadowCoverage coverage, ALNativeSourceWorkingSet& working_set,
    U64& serial, U32 frame)
try
{
    if (coverage == ALMetalWorldSpotShadowCoverage::opaque) return {};
    auto recipe = recipeFor(info,frame);if (!recipe) return {};
    const auto found = std::find_if(recipe->shadows.begin(),recipe->shadows.end(),
        [&](const auto& shadow) { return shadow.type == type && shadow.coverage == coverage; });
    if (found == recipe->shadows.end()) return {};
    const auto& state = info.metalSourcePublication();
    if (recipeAllocationForTesting) recipeAllocationForTesting(*state);
    if (state->identity() != recipe->identity || state->revision() != recipe->revision) return {};
    const auto next_serial = ++serial;
    auto record = found->source->restore(next_serial);
    if (!record) { ++recipe_statistics.expired_backing;return {}; }
    // Preserve all other live world/shadow variants. The CPU recipe never
    // owns them; a genuine current reader may own the native Payload.
    std::shared_ptr<const Payload> current;
    if (state->publication()) current = std::static_pointer_cast<const Payload>(state->publication());
    else if (info.mMetalShadowSource && info.mMetalShadowSource->native_scene_revision == recipe->revision)
        current = std::static_pointer_cast<const Payload>(info.mMetalShadowSource->native_scene_transient.lock());
    auto value = current && current->producer_revision == recipe->revision
        ? std::make_shared<Payload>(*current) : std::make_shared<Payload>();
    value->serial = next_serial;value->producer_revision = recipe->revision;
    value->vertices = record->vertexLease();value->indices = record->indexLease();
    // Cached sizes belong to exact weak native generations, so restoration
    // rescans neither texture mip/block descriptors nor mutable viewer inputs.
    account(*value,value->vertices,found->source->vertexBytes());
    account(*value,value->indices,found->source->indexBytes());
    for (std::size_t slot = 0; slot < record->textureLeases().size(); ++slot)
        account(*value,record->textureLeases()[slot],found->source->textureBytes(slot));
    std::erase_if(value->shadows,[&](const auto& shadow) { return shadow.type == type && shadow.coverage == coverage; });
    value->shadows.push_back({type,coverage,std::move(record)});
    if (state->identity() != recipe->identity || state->revision() != recipe->revision ||
        !publish(info,value,working_set,frame)) return {};
    ++recipe_statistics.shadow_restores;
    const auto* result = &value->shadows.back().record;
    return {std::move(value),result,nullptr,true};
}
catch (const std::bad_alloc&) { ++recipe_statistics.failures;return {}; }
}
