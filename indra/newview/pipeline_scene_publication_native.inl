/**
 * @file pipeline_scene_publication_native.inl
 * @brief Owner-sealed native object sources shared by main/probe/shadow views.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Include after ALViewerMetalShadowSourceCache is defined. This file contains
 * no live scene access on workers and no payload-destructor owner mutations.
 */
#include "alnativesourceworkingset.h"
#include "alnativeframebudget.h"
#include "almetalshadowprepare.h"
#include "almetalworldvisibility.h"
#include "almetalworldmaterialdata.h"
#include "almetalworldresources.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <utility>
#include <vector>
#include "../almetal/almetalworldcohort.h"
#include "../almetal/almetalworldanimated.h"

namespace ALScenePublicationNative
{
// Miss events count getter/factory calls, not unique objects, builds or
// failures. The cold factory calls current() again inside extend(), so getter
// and factory arrays are deliberately separate denominators. Hits add no work.
// A transient expiry identifies an unavailable same-revision reader; it does
// not assert which budget, withdrawal or lifetime decision caused that expiry.
enum class CurrentMissReason : U8
{
    first_encounter,
    producer_revision_changed,
    same_revision_transient_expired,
    count
};
enum class FactoryMissStage : U8
{
    current_payload_missing,
    variant_missing, // World flavor key, or the exact shadow type+coverage pair.
    optional_capability_promotion,
    count
};
struct SourceMissStats
{
    std::array<U64, static_cast<std::size_t>(CurrentMissReason::count)> current_misses{};
    std::array<U64, static_cast<std::size_t>(FactoryMissStage::count)> world_misses{}, shadow_misses{};
};
inline SourceMissStats source_miss_statistics;

enum class WorldFamily : U8 { simple, legacy, pbr };
inline std::unique_ptr<ALMetalWorldMaterialDataCache> material_data_cache;
inline U64 material_source_publications = 0, material_source_fallbacks = 0;
inline U64 resource_source_publications = 0, resource_source_fallbacks = 0;
inline U64 cohort_source_publications = 0;
inline U64 animated_source_publications = 0, animated_source_fallbacks = 0;
inline U64 animated_direct_source_publications = 0, animated_gpu_source_publications = 0;
struct AnimatedCompilationStats
{
    U64 requests=0,admitted=0,deferred=0,duplicate_views=0,published=0,failed=0;
    U64 stability_deferred=0;
    U64 requested_bytes=0,admitted_bytes=0,saturated=0;
};
inline AnimatedCompilationStats animated_compilation_stats;
// Owner-only deterministic allocation seam; never used by product settings
// or detached/GPU work. The current immutable source must survive optional
// promotion allocation failure without overwriting a newer publication.
inline void (*capabilityPromotionAllocationForTesting)(ALNativePublication::State&) = nullptr;
inline std::array<U64, static_cast<std::size_t>(ALMetalWorldResourceFallback::count)> resource_source_failure_reasons{};
inline ALMetalWorldResourceFallback resource_source_last_failure = ALMetalWorldResourceFallback::none;
inline void resourceSourceFailed(ALMetalWorldResourceFallback reason) noexcept
{
    ++resource_source_fallbacks;
    resource_source_last_failure = reason;
    const auto index = static_cast<std::size_t>(reason);
    if (index < resource_source_failure_reasons.size()) ++resource_source_failure_reasons[index];
}
inline void clearMaterialDataCache() noexcept { material_data_cache.reset(); }
inline ALMetalWorldMaterialDataCache::Stats materialDataStats() noexcept
{ return material_data_cache ? material_data_cache->stats() : ALMetalWorldMaterialDataCache::Stats{}; }


struct WorldFlavor
{
    WorldFamily family = WorldFamily::simple;
    bool indexed_textures = false, indexed_materials = false;
    bool visibility_bounds = false;
    bool native_materials = true;
    bool native_resources = false;
    // Optional immutable consumer capabilities, not shader/material semantics.
    bool cohort_source = false;
    bool cohort_bounds = false; // Optional CPU proof, never a shading-key bit.
    bool animated_source = false; // Pose-free CPU certificate, not a semantic key.
    bool animated_gpu_source = false; // Optional descriptor/bounds superset, also nonsemantic.
    bool stabilize_animated_direct = false; // Optional owner-stamp heuristic, never part of key().
    bool normal_map = false, specular_map = false;
    bool alpha_mask = false, rigged = false, pbr_glow = false;
    bool post_water_pbr_glow = false;
    bool impostor_fullbright = false, impostor_shiny = false, impostor_emboss = false;
    U32 material_alpha_mode = 0;
    bool hasResourceConsumer() const noexcept
    {
        return native_materials && family != WorldFamily::simple &&
            !(family == WorldFamily::legacy && material_alpha_mode == 1) &&
            !pbr_glow && !impostor_fullbright && !impostor_shiny && !impostor_emboss;
    }
    bool hasAnimatedConsumer() const noexcept
    {
        return rigged && (family==WorldFamily::legacy || family==WorldFamily::pbr) && hasResourceConsumer();
    }
    U64 key() const noexcept
    {
        return static_cast<U64>(family) | (U64(indexed_textures) << 2) |
            (U64(indexed_materials) << 3) | (U64(normal_map) << 4) |
            (U64(specular_map) << 5) | (U64(alpha_mask) << 6) |
            (U64(rigged) << 7) | (U64(pbr_glow) << 8) |
            (U64(impostor_fullbright) << 9) | (U64(impostor_shiny) << 10) |
            (U64(impostor_emboss) << 11) | (U64(visibility_bounds) << 12) |
            (U64(native_materials && family != WorldFamily::simple) << 13) |
            (U64(post_water_pbr_glow) << 15) |
            (U64(material_alpha_mode) << 32);
    }
};
// Owner-only fault seam for the optional per-view metadata transaction.
inline void (*visibilityMetadataAllocationForTesting)() = nullptr;

struct WorldVariant
{
    U64 key = 0;
    ALMetalWorldSimpleBatch prototype;
    std::shared_ptr<const ALMetalWorldCohortSource> cohort_source;
    std::shared_ptr<const ALMetalWorldAnimatedSource> animated_source;
    bool cpu_slots_frozen = false;
};
struct ShadowVariant
{
    U32 type = 0;
    ALMetalWorldSpotShadowCoverage coverage = ALMetalWorldSpotShadowCoverage::opaque;
    std::shared_ptr<const ALMetalShadowSourceRecord> record;
};
// The command-completion owner is a separate allocation. It owns only exact
// C++ native resource generations/uniforms, never source prototypes, viewer
// objects or a shared_ptr alias whose control block keeps a Payload alive.
struct NativeTuple
{
    std::vector<ALNativeSourceWorkingSet::Resource> resources;
};
inline std::shared_ptr<const NativeTuple> nativeTuple(
    const std::vector<ALNativeSourceWorkingSet::Resource>& resources,
    const std::shared_ptr<const NativeTuple>& previous = {})
{
    if (previous && previous->resources.size() == resources.size())
    {
        bool same = true;
        for (std::size_t i = 0; i < resources.size(); ++i)
        {
            const auto& a = previous->resources[i]; const auto& b = resources[i];
            if (a.bytes != b.bytes || a.allocation.get() != b.allocation.get() ||
                a.allocation.owner_before(b.allocation) || b.allocation.owner_before(a.allocation))
            { same = false; break; }
        }
        if (same) return previous;
    }
    auto value = std::make_shared<NativeTuple>();
    value->resources = resources;
    return value;
}
struct Payload
{
    U64 serial = 0, producer_revision = 0;
    ALMetalWorldConservativeBounds bounds;
    bool bounds_attempted = false;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    std::vector<WorldVariant> worlds;
    std::vector<ShadowVariant> shadows;
    std::vector<ALNativeSourceWorkingSet::Resource> resources;
    std::shared_ptr<const NativeTuple> native_tuple;
    // Deliberately no destructor callback: a last packet owner may disappear
    // on Metal completion, away from the scene/working-set publication owner.
};
struct WorldLease
{
    std::shared_ptr<const Payload> owner;
    const ALMetalWorldSimpleBatch* prototype = nullptr;
    const char* failure = nullptr;
    bool reused = false;
    explicit operator bool() const noexcept { return owner && prototype; }
};
struct ShadowLease
{
    std::shared_ptr<const Payload> owner;
    const std::shared_ptr<const ALMetalShadowSourceRecord>* record = nullptr;
    const char* failure = nullptr;
    bool reused = false;
    explicit operator bool() const noexcept { return owner && record && *record; }
};

inline U64 saturatedProduct(U64 a, U64 b) noexcept
{
    return b && a > std::numeric_limits<U64>::max() / b
        ? std::numeric_limits<U64>::max() : a * b;
}
inline U64 textureBytes(const ALMetalTexture& texture) noexcept
{
    const auto& d = texture.descriptor();
    U64 slices = d.array_length;
    if (d.type == ALMetalTextureType::texture_cube) slices = 6;
    else if (d.type == ALMetalTextureType::texture_cube_array) slices = saturatedProduct(slices, 6);
    U64 bytes = 0;
    for (U32 mip = 0; mip < d.mipmap_level_count; ++mip)
    {
        const U64 bw = llmax<U64>(texture.blockWidth(), 1);
        const U64 bh = llmax<U64>(texture.blockHeight(), 1);
        U64 level = saturatedProduct((U64(texture.width(mip)) + bw - 1) / bw,
                                    (U64(texture.height(mip)) + bh - 1) / bh);
        level = saturatedProduct(level, texture.bytesPerBlock());
        level = saturatedProduct(level, saturatedProduct(llmax<U64>(slices, 1), llmax<U64>(d.sample_count, 1)));
        if (level > std::numeric_limits<U64>::max() - bytes) return std::numeric_limits<U64>::max();
        bytes += level;
    }
    return bytes;
}
inline void account(Payload& value, std::shared_ptr<const void> allocation, U64 bytes)
{
    if (!allocation) return;
    for (const auto& resource : value.resources)
        if (!resource.allocation.owner_before(allocation) &&
            !allocation.owner_before(resource.allocation)) return;
    value.resources.push_back({std::move(allocation), bytes});
}

inline std::shared_ptr<const ALMetalTexture> imageLease(LLViewerTexture* texture)
{
    auto* image = texture ? texture->getGLTexture() : nullptr;
    auto lease = image ? image->getMetalTextureLease() : nullptr;
    if (!lease)
    {
        auto* fallback = LLViewerFetchedTexture::sDefaultImagep.get();
        image = fallback ? fallback->getGLTexture() : nullptr;
        lease = image ? image->getMetalTextureLease() : nullptr;
    }
    return lease;
}
inline const ALMetalTexture* resolve(Payload& value, LLViewerTexture* texture)
{
    auto lease = imageLease(texture);
    if (!lease) return nullptr;
    const auto* result = lease.get();
    const U64 bytes = textureBytes(*lease);
    account(value, std::move(lease), bytes);
    return result;
}

inline std::shared_ptr<const Payload> current(const LLDrawInfo& info)
{
    const auto& state = info.metalSourcePublication();
    if (!state->dirty() && state->publication())
        return std::static_pointer_cast<const Payload>(state->publication());
    const auto& metadata = info.mMetalShadowSource;
    if (metadata && metadata->native_scene_revision == state->revision())
    {
        auto value = std::static_pointer_cast<const Payload>(metadata->native_scene_transient.lock());
        if (!value) ++source_miss_statistics.current_misses[
            static_cast<std::size_t>(CurrentMissReason::same_revision_transient_expired)];
        return value;
    }
    ++source_miss_statistics.current_misses[static_cast<std::size_t>(
        metadata && metadata->native_scene_revision
            ? CurrentMissReason::producer_revision_changed : CurrentMissReason::first_encounter)];
    return {};
}
inline const ALMetalWorldSimpleBatch* findWorld(const Payload& value, U64 key) noexcept
{
    for (const auto& variant : value.worlds)
        if (variant.key == key) return &variant.prototype;
    return nullptr;
}
inline const WorldVariant* findWorldVariant(const Payload& value, U64 key) noexcept
{
    for (const auto& variant : value.worlds) if (variant.key == key) return &variant;
    return nullptr;
}
inline const std::shared_ptr<const ALMetalShadowSourceRecord>* findShadow(
    const Payload& value, U32 type, ALMetalWorldSpotShadowCoverage coverage) noexcept
{
    for (const auto& variant : value.shadows)
        if (variant.type == type && variant.coverage == coverage) return &variant.record;
    return nullptr;
}
inline bool geometry(const LLDrawInfo& info)
{
    auto* buffer = info.mVertexBuffer.get();
    return info.mCount && buffer && buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) &&
        (buffer->getIndexStride() == 2 || buffer->getIndexStride() == 4) &&
        buffer->prepareMetalBuffers(true) &&
        buffer->validateRange(info.mStart, info.mEnd, info.mCount, info.mOffset);
}
inline std::shared_ptr<Payload> extend(const LLDrawInfo& info, U64& serial)
{
    auto previous = current(info);
    auto value = previous ? std::make_shared<Payload>(*previous) : std::make_shared<Payload>();
    value->serial = ++serial;
    value->producer_revision = info.metalSourcePublication()->revision();
    if (!value->vertices)
    {
        value->vertices = info.mVertexBuffer->getMetalVertexBufferLease();
        value->indices = info.mVertexBuffer->getMetalIndexBufferLease();
        if (!value->vertices || !value->indices) return {};
        account(*value, value->vertices, value->vertices->length());
        account(*value, value->indices, value->indices->length());
    }
    return value;
}
inline void qualifyBounds(const LLDrawInfo& info, const ALMetalWorldSimpleBatch& batch, Payload& payload)
{
    if (payload.bounds_attempted) return;
    payload.bounds_attempted = true;
    // Exact published rigid position plane, in the same local coordinates
    // consumed by the vertex shader. Unproved/animated extents fail visible.
    if (!payload.vertices || !payload.indices || batch.vertices != payload.vertices.get() ||
        batch.indices != payload.indices.get() || info.mAvatar || info.mSkinInfo ||
        batch.weight_offset || batch.cloth_offset || batch.avatar_rigid || batch.avatar_skinning || batch.avatar_cloth) return;
    const auto* bytes = static_cast<const U8*>(payload.vertices->mappedBytes());
    const auto* indices = static_cast<const U8*>(payload.indices->mappedBytes());
    // Prefer the established cached CPU shadow over reading write-combined
    // Metal storage, but only while it proves this exact published generation.
    // A reentrant/new producer falls back to the pinned immutable native bytes.
    const auto* buffer=info.mVertexBuffer.get();
    if(buffer && info.metalSourcePublication()->revision()==payload.producer_revision &&
       buffer->hasPublishedMetalVertexBuffer() && buffer->hasPublishedMetalIndexBuffer() &&
       buffer->getMetalVertexBuffer()==payload.vertices.get() && buffer->getMetalIndexBuffer()==payload.indices.get() &&
       buffer->getSize()>=payload.vertices->length() && buffer->getIndicesSize()>=payload.indices->length())
    {
        if(buffer->getMappedData())bytes=buffer->getMappedData();
        if(buffer->getMappedIndices())indices=buffer->getMappedIndices();
    }
    const auto stride = batch.index_type == ALMetalIndexType::uint32 ? 4u : 2u;
    if (bytes && indices && batch.vertex_count && batch.index_count &&
        batch.position_offset <= payload.vertices->length() &&
        batch.vertex_count <= (payload.vertices->length() - batch.position_offset) / 16 &&
        batch.index_offset <= payload.indices->length() &&
        batch.index_count <= (payload.indices->length() - batch.index_offset) / stride)
    {
        bool finite = true;
        for (U32 axis = 0; axis < 3; ++axis)
        {
            payload.bounds.minimum[axis] = std::numeric_limits<float>::infinity();
            payload.bounds.maximum[axis] = -std::numeric_limits<float>::infinity();
        }
        for (U32 at = 0; at < batch.index_count; ++at)
        {
            U32 vertex = 0;
            const auto* index = indices + batch.index_offset + std::size_t(at) * stride;
            if (stride == 2)
            { U16 small; std::memcpy(&small, index, sizeof(small)); vertex = small; }
            else std::memcpy(&vertex, index, sizeof(vertex));
            if (vertex >= batch.vertex_count) { finite = false; break; }
            float point[4]; std::memcpy(point, bytes + batch.position_offset + std::size_t(vertex) * 16, sizeof(point));
            for (U32 axis = 0; axis < 3; ++axis)
            {
                finite = finite && std::isfinite(point[axis]);
                payload.bounds.minimum[axis] = std::min(payload.bounds.minimum[axis], point[axis]);
                payload.bounds.maximum[axis] = std::max(payload.bounds.maximum[axis], point[axis]);
            }
        }
        payload.bounds.qualified = finite;
    }
}

inline bool publish(const LLDrawInfo& info, const std::shared_ptr<Payload>& value,
                    ALNativeSourceWorkingSet& working_set, U32 frame)
{
    const auto& state = info.metalSourcePublication();
    if (!info.mMetalShadowSource)
        info.mMetalShadowSource = std::make_shared<ALViewerMetalShadowSourceCache>();
    // Dirty publication pays this exact-union metadata copy once; adding a
    // CPU-only variant can reuse the identical already-sealed native tuple.
    value->native_tuple = nativeTuple(value->resources, value->native_tuple);
    if (!state->publish(value->producer_revision, value)) return false;
    auto& metadata = *info.mMetalShadowSource;
    metadata.native_scene_revision = value->producer_revision;
    metadata.native_scene_transient = value;
    const std::shared_ptr<const void> expected = value;
    // The caller already owns value. Rejection/eviction drops only State's
    // publisher lease; actual packets may still reuse the weak transient.
    working_set.retain(state, value->serial, expected, value->resources, frame);
    // Cache withdrawal alone still permits the caller's transient reader.
    // An actual producer change during residency callbacks rejects this work
    // without clearing a newer reentrant publication or its metadata.
    return state->revision() == value->producer_revision;
}

inline bool buildWorld(const LLDrawInfo& info, const WorldFlavor& flavor,
                       Payload& value, ALMetalWorldSimpleBatch& batch)
{
    if (flavor.post_water_pbr_glow && (flavor.family != WorldFamily::pbr || !flavor.pbr_glow)) return false;
    auto* buffer = info.mVertexBuffer.get();
    const auto has = [&](LLVertexBuffer::AttributeType type) { return buffer->hasDataType(type); };
    if ((!flavor.impostor_fullbright && !has(LLVertexBuffer::TYPE_NORMAL)) ||
        !has(LLVertexBuffer::TYPE_TEXCOORD0) || !has(LLVertexBuffer::TYPE_COLOR) ||
        (flavor.rigged && (!info.mAvatar || !info.mSkinInfo || !has(LLVertexBuffer::TYPE_WEIGHT4))))
        return false;
    batch.vertices = value.vertices.get(); batch.indices = value.indices.get();
    batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
    batch.normal_offset = has(LLVertexBuffer::TYPE_NORMAL) ? buffer->getOffset(LLVertexBuffer::TYPE_NORMAL) : 0;
    batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
    batch.color_offset = buffer->getOffset(LLVertexBuffer::TYPE_COLOR);
    batch.weight_offset = flavor.rigged ? buffer->getOffset(LLVertexBuffer::TYPE_WEIGHT4) : 0;
    batch.vertex_count = buffer->getNumVerts();
    batch.index_offset = std::size_t(info.mOffset) * buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4 ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = info.mCount;
    batch.alpha_mask = flavor.alpha_mask;
    batch.alpha_cutoff = flavor.alpha_mask ? info.mAlphaMaskCutoff : 0.f;
    batch.impostor_fullbright = flavor.impostor_fullbright;
    batch.impostor_shiny = flavor.impostor_shiny;
    batch.impostor_emboss = flavor.impostor_emboss;
    const auto white = [] { return LLViewerFetchedTexture::sWhiteImagep.get(); };
    const auto flat = [] { return LLViewerFetchedTexture::sFlatNormalImagep.get(); };
    const bool indexed = flavor.family == WorldFamily::simple
        ? flavor.indexed_textures : flavor.indexed_materials;
    if (indexed && !has(LLVertexBuffer::TYPE_TEXTURE_INDEX)) return false;

    if (flavor.family == WorldFamily::simple)
    {
        const auto count = indexed ? info.mTextureList.size() : 1;
        if (!count || count > batch.max_diffuse_textures) return false;
        batch.texture_count = static_cast<U32>(count);
        for (std::size_t slot = 0; slot < count; ++slot)
        {
            auto* texture = indexed ? info.mTextureList[slot].get() : info.mTexture.get();
            batch.diffuse_textures[slot] = resolve(value, texture ? texture : white());
            if (!batch.diffuse_textures[slot]) return false;
        }
        return true;
    }
    const auto count = indexed ? (flavor.family == WorldFamily::legacy
        ? info.mMaterialSlotList.size() : info.mGLTFMaterialList.size()) : 1;
    if (!count || count > batch.max_material_slots || (indexed && count < 2)) return false;
    batch.texture_count = static_cast<U32>(count);
    if (flavor.family == WorldFamily::legacy)
    {
        if ((flavor.normal_map && (!has(LLVertexBuffer::TYPE_TANGENT) || !has(LLVertexBuffer::TYPE_TEXCOORD1))) ||
            (flavor.specular_map && !has(LLVertexBuffer::TYPE_TEXCOORD2))) return false;
        batch.material_slot_count = static_cast<U32>(count);
        batch.material_alpha_mode = flavor.material_alpha_mode;
        if (flavor.normal_map)
        {
            batch.tangent_offset = buffer->getOffset(LLVertexBuffer::TYPE_TANGENT);
            batch.texcoord1_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD1);
        }
        if (flavor.specular_map) batch.texcoord2_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD2);
        for (std::size_t slot = 0; slot < count; ++slot)
        {
            const auto* source = indexed ? &info.mMaterialSlotList[slot] : nullptr;
            auto* diffuse = source ? source->mDiffuse.get() : info.mTexture.get();
            auto* normal = source ? source->mNormalMap.get() : info.mNormalMap.get();
            auto* specular = source ? source->mSpecularMap.get() : info.mSpecularMap.get();
            auto& destination = batch.materialSlots()[slot];
            const auto& color = source ? source->mSpecColor : info.mSpecColor;
            std::memcpy(destination.specular_color, color.mV, sizeof(destination.specular_color));
            destination.env_intensity = source ? source->mEnvIntensity : info.mEnvIntensity;
            destination.alpha_cutoff = source ? source->mAlphaMaskCutoff : info.mAlphaMaskCutoff;
            destination.fullbright = source ? source->mFullbright : (info.mFullbright ? 1.f : 0.f);
            batch.diffuse_textures[slot] = resolve(value, diffuse ? diffuse : white());
            if (!batch.diffuse_textures[slot]) return false;
            if (flavor.normal_map)
            {
                destination.normal_texture = resolve(value, normal ? normal : flat());
                if (!destination.normal_texture) return false;
            }
            if (flavor.specular_map)
            {
                destination.specular_texture = resolve(value, specular ? specular : white());
                if (!destination.specular_texture) return false;
            }
        }
        return true;
    }
    if (!has(LLVertexBuffer::TYPE_TANGENT) ||
        (flavor.pbr_glow && !has(LLVertexBuffer::TYPE_EMISSIVE)) ||
        (!indexed && !info.mGLTFMaterial)) return false;
    batch.tangent_offset = buffer->getOffset(LLVertexBuffer::TYPE_TANGENT);
    batch.pbr_glow = flavor.pbr_glow;
    if (flavor.pbr_glow) batch.emissive_offset = buffer->getOffset(LLVertexBuffer::TYPE_EMISSIVE);
    batch.pbr_slot_count = static_cast<U32>(count);
    for (std::size_t slot = 0; slot < count; ++slot)
    {
        auto* material = indexed ? info.mGLTFMaterialList[slot].get() : info.mGLTFMaterial.get();
        auto& destination = batch.pbrSlots()[slot];
        LLViewerTexture* base = !indexed ? info.mTexture.get() : nullptr;
        LLViewerTexture* emissive = base;
        LLViewerTexture* normal = flat();
        LLViewerTexture* orm = white();
        if (material)
        {
            const bool masked = material->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK;
            if (!flavor.pbr_glow && masked != flavor.alpha_mask) return false;
            if (!base) base = material->getRenderTexture(LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR);
            normal = material->getRenderTexture(LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL);
            if (!normal || normal->getDiscardLevel() > 4) normal = flat();
            orm = material->getRenderTexture(LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS);
            if (!emissive) emissive = material->getRenderTexture(LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE);
            for (U32 texture = 0; texture < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++texture)
            {
                LLGLTFMaterial::TextureTransform::Pack packed;
                material->mTextureTransform[texture].getPacked(packed);
                std::memcpy(destination.texture_transform[texture], packed, sizeof(packed));
            }
            std::memcpy(destination.emissive_color, material->mEmissiveColor.mV, sizeof(material->mEmissiveColor.mV));
            std::memcpy(destination.base_color_factor, material->mBaseColor.mV, sizeof(destination.base_color_factor));
            destination.base_color_factor[3] = material->getStreamedBaseAlpha();
            destination.roughness_factor = material->mRoughnessFactor;
            destination.metallic_factor = material->mMetallicFactor;
            destination.alpha_cutoff = masked ? material->mAlphaCutoff : -1.f;
            batch.double_sided = batch.double_sided || material->mDoubleSided;
        }
        if (flavor.post_water_pbr_glow) destination.alpha_cutoff = 0.004f;
        batch.diffuse_textures[slot] = resolve(value, base ? base : white());
        destination.normal_texture = resolve(value, normal ? normal : flat());
        destination.orm_texture = resolve(value, orm ? orm : white());
        destination.emissive_texture = resolve(value, emissive ? emissive : white());
        if (!batch.diffuse_textures[slot] || !destination.normal_texture ||
            !destination.orm_texture || !destination.emissive_texture) return false;
    }
    return true;
}

// Recover typed aliases only from the exact completed producer union. Raw
// texture addresses never manufacture ownership and buffers are never cast.
inline bool collectWorldImages(const Payload& value, const ALMetalWorldSimpleBatch& batch,
                               std::vector<std::shared_ptr<const ALMetalTexture>>& images)
{
    if (!batch.texture_count || batch.texture_count > batch.diffuse_textures.size() ||
        batch.material_slot_count > batch.max_material_slots || batch.pbr_slot_count > batch.max_material_slots) return false;
    images.reserve(batch.texture_count * 4);
    const auto image = [&](const ALMetalTexture* texture)
    {
        if (!texture) return true;
        for (const auto& existing : images) if (existing.get() == texture) return true;
        for (const auto& resource : value.resources)
            if (resource.allocation.get() == texture)
            {
                auto typed = std::shared_ptr<const ALMetalTexture>(resource.allocation, texture);
                if (resource.bytes != textureBytes(*typed)) return false;
                images.push_back(std::move(typed));return true;
            }
        return false;
    };
    for (U32 slot = 0; slot < batch.texture_count; ++slot)
        if (!batch.diffuse_textures[slot] || !image(batch.diffuse_textures[slot])) return false;
    for (U32 slot = 0; slot < batch.material_slot_count; ++slot)
        if (!image(batch.materialSlots()[slot].normal_texture) || !image(batch.materialSlots()[slot].specular_texture)) return false;
    for (U32 slot = 0; slot < batch.pbr_slot_count; ++slot)
        if (!image(batch.pbrSlots()[slot].normal_texture) || !image(batch.pbrSlots()[slot].orm_texture) ||
            !image(batch.pbrSlots()[slot].emissive_texture)) return false;
    return true;
}
inline bool completeWorldResources(const Payload& value, const ALMetalWorldSimpleBatch& batch)
{
    const auto buffer = [&](const std::shared_ptr<const ALMetalBuffer>& allocation)
    {
        if (!allocation) return true;
        for (const auto& resource : value.resources)
            if (resource.allocation.get() == allocation.get() && resource.bytes == allocation->length() &&
                !resource.allocation.owner_before(allocation) && !allocation.owner_before(resource.allocation)) return true;
        return false;
    };
    if (value.vertices.get() != batch.vertices || value.indices.get() != batch.indices ||
        !buffer(value.vertices) || !buffer(value.indices)) return false;
    if (batch.native_material_source &&
        (!buffer(batch.native_material_source->materialLease()) || !buffer(batch.native_material_source->pbrLease(false)) ||
         !buffer(batch.native_material_source->pbrLease(true)))) return false;
    std::vector<std::shared_ptr<const ALMetalTexture>> images;
    return collectWorldImages(value, batch, images);
}
inline void publishResourceSource(Payload& value, ALMetalWorldSimpleBatch& prototype)
{
    try
    {
        std::vector<std::shared_ptr<const ALMetalTexture>> images;
        if (!collectWorldImages(value, prototype, images))
        { resourceSourceFailed(ALMetalWorldResourceFallback::missing_source);return; }
        auto* device = LLImageGL::getMetalDevice();
        auto failure = ALMetalWorldResourceFallback::unsupported;
        if (device && ALMetalWorldResourceSource::publish(*device, prototype, images, nullptr, &failure))
            ++resource_source_publications;
        else resourceSourceFailed(failure);
    }
    catch (const std::bad_alloc&) { resourceSourceFailed(ALMetalWorldResourceFallback::host_allocation); }
}
inline void sealCohortSource(const Payload& value, WorldVariant& variant, U64 identity, U64 revision)
{
    const auto& batch = variant.prototype;
    if (variant.cohort_source)
    {
        const auto& prior = variant.cohort_source->source();
        const auto& key = variant.cohort_source->key();
        if (key.identity == identity && key.flavor == variant.key && key.revision == revision &&
            (!value.bounds_attempted || variant.cohort_source->hasBoundsPublication()) &&
            prior.vertices == batch.vertices && prior.indices == batch.indices &&
            prior.native_material_source == batch.native_material_source && prior.texture_count == batch.texture_count &&
            prior.texture_count <= prior.diffuse_textures.size() &&
            std::equal(prior.diffuse_textures.begin(), prior.diffuse_textures.begin() + prior.texture_count, batch.diffuse_textures.begin())) return;
        variant.cohort_source.reset();
    }
    if (!completeWorldResources(value, batch)) return;
    variant.cohort_source = ALMetalWorldCohortSource::seal({{identity, variant.key, revision}, &batch}, nullptr,
        value.bounds_attempted ? &value.bounds : nullptr);
    if (variant.cohort_source) ++cohort_source_publications;
}

inline bool animatedSourceReady(const Payload& value,const WorldVariant& variant,bool gpu_requested=false) noexcept
{
    if (!variant.animated_source || !value.native_tuple) return false;
    const auto& source = *variant.animated_source;
    const auto& key = source.key();
    const auto& prototype = source.prototype();
    return key.flavor == variant.key && key.revision == value.producer_revision &&
        prototype.vertices == variant.prototype.vertices && prototype.indices == variant.prototype.indices &&
        prototype.vertices == value.vertices.get() && prototype.indices == value.indices.get() &&
        prototype.weight_offset == variant.prototype.weight_offset &&
        prototype.native_material_source == variant.prototype.native_material_source &&
        // Argument descriptors do not affect the ordinary direct consumer.
        // A descriptor-only promotion need not discard its existing CPU proof.
        (!gpu_requested || (source.gpuReady() &&
            prototype.native_resource_source == variant.prototype.native_resource_source)) &&
        source.matchesNativeOwner(value.native_tuple);
}
inline const std::shared_ptr<const ALMetalWorldAnimatedSource>* animatedSourceFor(
    const WorldLease& lease,const WorldFlavor& flavor) noexcept
{
    if (!lease) return nullptr;
    const auto* variant = findWorldVariant(*lease.owner,flavor.key());
    // A denied optional GPU upgrade still emits this completed CPU source;
    // the renderer can consume it directly with the SAME current palette.
    return variant && &variant->prototype == lease.prototype && animatedSourceReady(*lease.owner,*variant)
        ? &variant->animated_source : nullptr;
}
inline U64 animatedCompilationWorkBytes(const ALMetalWorldSimpleBatch& source,bool gpu_requested=true) noexcept
{
    // Checked logical compile/copy/proof work, not resident geometry/texture
    // bytes, physical heap charge, or a guarantee of CPU execution time.
    const U64 descriptors=sizeof(ALMetalWorldSimpleBatch)+sizeof(ALMetalWorldAnimatedSource)+
        sizeof(ALMetalWorldSourceNativeScope)+(gpu_requested?110*sizeof(ALMetalWorldAnimatedBoneBounds):0);
    if(!gpu_requested)return descriptors;
    const U64 scan=saturatedProduct(source.index_count,2*sizeof(float)*4+
        (source.index_type==ALMetalIndexType::uint32?4:2));
    return scan>=ALNativeFrameBudget::unknown_bytes-descriptors
        ? ALNativeFrameBudget::unknown_bytes : descriptors+scan;
}
struct AnimatedCompilationTicket
{
    // CPU-local only. This owner is never stored in a Payload, FrameView or
    // native completion callback; it survives owner reentrancy until return.
    std::shared_ptr<ALViewerMetalShadowSourceCache> owner;
    ALNativeFrameBudget::Lease credit;
    bool completed=false;
    AnimatedCompilationTicket()=default;
    AnimatedCompilationTicket(const AnimatedCompilationTicket&)=delete;
    AnimatedCompilationTicket& operator=(const AnimatedCompilationTicket&)=delete;
    ~AnimatedCompilationTicket()
    {if(credit && !completed)++animated_compilation_stats.failed;}
    void publicationCompleted() noexcept
    {
        if(credit && owner && owner->animated_compilation && !completed)
        {owner->animated_compilation->publicationCompleted();completed=true;++animated_compilation_stats.published;}
    }
};
inline bool admitAnimatedCompilation(const LLDrawInfo& info,const ALMetalWorldSimpleBatch& source,
                                     ALNativeFrameBudget& budget,AnimatedCompilationTicket& ticket,bool gpu_requested=true,
                                     bool stabilize_direct=false) noexcept
try
{
    auto& stats=animated_compilation_stats;
    const bool require_stability=stabilize_direct && !gpu_requested;
    U64 bytes=0;
    if(!require_stability)
    {
        // Preserve the existing immediate/GPU path's checked-work ordering.
        bytes=animatedCompilationWorkBytes(source,gpu_requested);
        if(bytes==ALNativeFrameBudget::unknown_bytes){++stats.saturated;++stats.deferred;return false;}
    }
    // Only the existing scalar admission state is needed to remember the
    // first observation; no optional source/descriptor is allocated here.
    if(!info.mMetalShadowSource)info.mMetalShadowSource=std::make_shared<ALViewerMetalShadowSourceCache>();
    ticket.owner=info.mMetalShadowSource;
    if(!ticket.owner->animated_compilation)
        ticket.owner->animated_compilation=std::make_unique<ALNativeCohortCompilationAdmission>();
    auto& owner=*ticket.owner->animated_compilation;
    const auto frame=budget.snapshot().frame;
    if(owner.lastConsideredFrame() && frame<=*owner.lastConsideredFrame())
    {
        ++stats.duplicate_views;
        if(!require_stability)++stats.deferred;
        return false;
    }
    if(require_stability)
    {
        const auto& state=info.metalSourcePublication();
        // This observes only the existing producer stamp, not every child.
        // Required raw publication remains current; the eventual admitted
        // seal still proves its actual native generations. Main/probe views
        // in one outer frame cannot supply the second observation.
        if(owner.deferUnstableRevision(frame,ALNativeCohortCompilationAdmission::ProducerStamp{
            state->identity(),state->revision()}))
        {++stats.stability_deferred;return false;}
        bytes=animatedCompilationWorkBytes(source,false);
        if(bytes==ALNativeFrameBudget::unknown_bytes){++stats.saturated;++stats.deferred;return false;}
    }
    ++stats.requests;stats.requested_bytes+=bytes;
    auto decision=owner.tryAcquire(budget,bytes);
    if(!decision){++stats.deferred;return false;}
    ticket.credit=std::move(decision.credit);++stats.admitted;stats.admitted_bytes+=bytes;
    return true;
}
catch(const std::bad_alloc&){++animated_compilation_stats.deferred;return false;}
inline void sealAnimatedSource(Payload& value,WorldVariant& variant,U64 identity,U64 revision,bool gpu_requested) noexcept
try
{
    // The certificate owns no pose, view, avatar or parent publication. Its
    // geometry/weight plane and material facts belong to this exact completed
    // producer generation; later camera/pose changes cannot alter its key.
    // Direct-only sealing proves the static tuple without reading indices or
    // bind-space positions/weights. GPU bounds remain an admitted capability.
    value.native_tuple = nativeTuple(value.resources,value.native_tuple);
    if (animatedSourceReady(value,variant,gpu_requested)) return;
    if (!completeWorldResources(value,variant.prototype))
    { ++animated_source_fallbacks;return; }
    auto source = ALMetalWorldAnimatedSource::seal({identity,variant.key,revision},variant.prototype,
        {value.native_tuple,value.vertices,value.indices},nullptr,gpu_requested);
    if (!source) { ++animated_source_fallbacks;return; }
    if(source->gpuReady())++animated_gpu_source_publications;
    else ++animated_direct_source_publications;
    variant.animated_source = std::move(source);
    ++animated_source_publications;
}
catch (const std::bad_alloc&) { ++animated_source_fallbacks; }

inline WorldLease world(const LLDrawInfo& info, WorldFlavor flavor,
                        ALNativeSourceWorkingSet& working_set, U64& serial, U32 frame,
                        ALNativeFrameBudget* compilation_budget=nullptr)
{
    try
    {
        std::optional<AnimatedCompilationTicket> animated_ticket;
        info.observeMetalSourceDependencies();
        if (flavor.family == WorldFamily::simple)
            flavor.indexed_textures = flavor.indexed_textures && info.mTextureList.size() > 1;
        flavor.animated_source = (flavor.animated_source || flavor.animated_gpu_source) && flavor.hasAnimatedConsumer();
        flavor.animated_gpu_source = flavor.animated_gpu_source && flavor.animated_source;
        flavor.stabilize_animated_direct = flavor.stabilize_animated_direct && flavor.animated_source && !flavor.animated_gpu_source;
        flavor.native_resources = (flavor.native_resources || flavor.animated_gpu_source) && flavor.hasResourceConsumer();
        const U64 key = flavor.key();
        if (auto value = current(info))
        {
            if (const auto* found = findWorldVariant(*value, key))
            {
                const auto needs_cpu_freeze = [&](const WorldVariant& variant)
                {
                    return !flavor.native_materials &&
                        (variant.prototype.material_slot_count || variant.prototype.pbr_slot_count) && !variant.cpu_slots_frozen;
                };
                const auto ready = [&](const WorldVariant& variant, const Payload& publication)
                {
                    return (!flavor.native_resources || (variant.prototype.native_resource_source &&
                        variant.prototype.native_resource_source->matches(variant.prototype))) &&
                        (!flavor.cohort_bounds || publication.bounds_attempted) &&
                        (!flavor.cohort_source || (variant.cohort_source &&
                            (!flavor.cohort_bounds || variant.cohort_source->hasBoundsPublication()))) &&
                        (!flavor.animated_source || animatedSourceReady(publication,variant,flavor.animated_gpu_source)) && !needs_cpu_freeze(variant);
                };
                if (ready(*found, *value))
                {
                    working_set.touch(info.metalSourcePublication()->identity(), value->serial, frame);
                    return {std::move(value), &found->prototype, nullptr, true};
                }
                ++source_miss_statistics.world_misses[
                    static_cast<std::size_t>(FactoryMissStage::optional_capability_promotion)];
                // Resource/cohort capabilities do not change shader semantics.
                // Promote a completed immutable variant from its exact native
                // tuple, without rebuilding LL geometry/material/texture data.
                const auto state = info.metalSourcePublication();
                const auto identity = state->identity();
                const auto revision = value->producer_revision;
                const auto original_current = [&]
                { return state->identity() == identity && state->revision() == revision && state->publication().get() == value.get(); };
                const auto authored_transient_current = [&]
                {
                    if ((!needs_cpu_freeze(*found) && !flavor.cohort_bounds) || flavor.native_resources || flavor.cohort_source ||
                        state->publication() || state->identity() != identity || state->revision() != revision ||
                        info.metalSourcePublication() != state) return false;
                    const auto& metadata = info.mMetalShadowSource;
                    if (!metadata || metadata->native_scene_revision != revision) return false;
                    auto reader = metadata->native_scene_transient.lock();
                    return reader && reader.get() == value.get() &&
                        !reader.owner_before(value) && !value.owner_before(reader);
                };
                const auto animated_transient_current = [&]
                {
                    // PPD-289: an optional animated certificate cannot turn
                    // cache-only withdrawal of this real reader into missing
                    // required geometry. Do not promote, charge or republish
                    // a child here; the caller keeps the ordinary raw route.
                    if (!flavor.animated_source || state->publication() ||
                        state->identity()!=identity || state->revision()!=revision) return false;
                    const auto& actual=info.metalSourcePublication();
                    if(actual.get()!=state.get() || actual.owner_before(state) || state.owner_before(actual)) return false;
                    const auto& metadata=info.mMetalShadowSource;
                    if(!metadata || metadata->native_scene_revision!=revision) return false;
                    auto reader=metadata->native_scene_transient.lock();
                    return reader && reader.get()==value.get() &&
                        !reader.owner_before(value) && !value.owner_before(reader);
                };
                try
                {
                    if (capabilityPromotionAllocationForTesting) capabilityPromotionAllocationForTesting(*state);
                    if (animated_transient_current())
                        return {std::move(value), &found->prototype, nullptr, true};
                    if (flavor.animated_source && !original_current())
                        return {{}, nullptr, "Native scene changed before optional animated publication"};
                    // Cache withdrawal may leave only this real reader. CPU
                    // freezing/bounds are optional: retain its exact source
                    // (unqualified bounds fail visible), without resurrecting
                    // an unbudgeted child publisher. PPD-205 / PPD-219.
                    if (authored_transient_current())
                        return {std::move(value), &found->prototype, nullptr, true};
                    if(flavor.animated_source && !animatedSourceReady(*value,*found,flavor.animated_gpu_source))
                    {
                        animated_ticket.emplace();
                        if(!admitAnimatedCompilation(info,found->prototype,
                            compilation_budget?*compilation_budget:ALNativeFrameBudget::instance(),*animated_ticket,
                            flavor.animated_gpu_source,flavor.stabilize_animated_direct))
                            return {std::move(value), &found->prototype, nullptr, true};
                    }
                    const auto index = static_cast<std::size_t>(found - value->worlds.data());
                    auto promoted = std::make_shared<Payload>(*value);
                    auto& variant = promoted->worlds[index];
                    if (flavor.cohort_bounds) qualifyBounds(info, variant.prototype, *promoted);
                    if (needs_cpu_freeze(variant)) variant.cpu_slots_frozen = variant.prototype.freezeCpuSlots();
                    const bool had_cohort_source = static_cast<bool>(variant.cohort_source);
                    if (flavor.native_resources && (!variant.prototype.native_resource_source ||
                        !variant.prototype.native_resource_source->matches(variant.prototype)))
                        publishResourceSource(*promoted, variant.prototype);
                    if (flavor.cohort_source || had_cohort_source)
                        sealCohortSource(*promoted, variant, info.metalSourcePublication()->identity(), promoted->producer_revision);
                    if (flavor.animated_source)
                        sealAnimatedSource(*promoted,variant,identity,promoted->producer_revision,flavor.animated_gpu_source);
                    if (!ready(variant, *promoted))
                    {
                        if (!original_current())
                            return {{}, nullptr, "Native scene changed while optional capabilities were unavailable"};
                        working_set.touch(info.metalSourcePublication()->identity(), value->serial, frame);
                        return {std::move(value), &found->prototype, nullptr, true};
                    }
                    if (!original_current())
                        return {{}, nullptr, "Native scene publication changed while promoting capabilities"};
                    promoted->serial = ++serial;
                    if (!publish(info, promoted, working_set, frame))
                        return {{}, nullptr, "Native scene producer changed while promoting capabilities"};
                    if(animated_ticket && animatedSourceReady(*promoted,variant,flavor.animated_gpu_source))animated_ticket->publicationCompleted();
                    const auto* prototype = &promoted->worlds[index].prototype;
                    return {std::move(promoted), prototype, nullptr, true};
                }
                catch (const std::bad_alloc&)
                {
                    if (original_current() || authored_transient_current() || animated_transient_current())
                        return {std::move(value), &found->prototype, nullptr, true};
                    return {{}, nullptr, "Native scene changed while optional promotion allocation failed"};
                }
            }
            ++source_miss_statistics.world_misses[
                static_cast<std::size_t>(FactoryMissStage::variant_missing)];
        }
        else ++source_miss_statistics.world_misses[
            static_cast<std::size_t>(FactoryMissStage::current_payload_missing)];
        if (!geometry(info)) return {{}, nullptr, "Native scene geometry is unpublished or invalid"};
        info.observeMetalSourceDependencies();
        auto value = extend(info, serial);
        if (!value) return {{}, nullptr, "Native scene geometry lease is missing"};
        auto& variant = value->worlds.emplace_back();
        variant.key = key;
        if (!buildWorld(info, flavor, *value, variant.prototype))
            return {{}, nullptr, "Native scene world material/layout is incomplete"};
        if (flavor.visibility_bounds || flavor.cohort_bounds) qualifyBounds(info, variant.prototype, *value);
        if (!flavor.native_materials) variant.cpu_slots_frozen = variant.prototype.freezeCpuSlots();
        if (flavor.native_materials && flavor.family != WorldFamily::simple)
        {
            // Optional native table publication never removes required draws.
            // Failures preserve the existing native per-view packing path.
            try
            {
                // Reserve accounting before publication mutates the staged
                // prototype, keeping optional OOM fallback transactional.
                value->resources.reserve(value->resources.size() + 3);
                auto* device = LLImageGL::getMetalDevice();
                if (device)
                {
                    if (!material_data_cache || !material_data_cache->belongsTo(*device))
                        material_data_cache = std::make_unique<ALMetalWorldMaterialDataCache>(*device);
                    auto source = ALMetalWorldMaterialSource::publish(*device, variant.prototype, *material_data_cache);
                    if (source)
                    {
                        const auto retain = [&](std::shared_ptr<const ALMetalBuffer> lease)
                        { if (lease) account(*value, lease, lease->length()); };
                        retain(source->materialLease());
                        retain(source->pbrLease(false));
                        retain(source->pbrLease(true));
                        ++material_source_publications;
                    }
                    else ++material_source_fallbacks;
                }
                else ++material_source_fallbacks;
            }
            catch (const std::bad_alloc&) { ++material_source_fallbacks; }
        }
        bool animated_admitted=true;
        if(flavor.animated_source)
        {animated_ticket.emplace();animated_admitted=admitAnimatedCompilation(info,variant.prototype,
            compilation_budget?*compilation_budget:ALNativeFrameBudget::instance(),*animated_ticket,
            flavor.animated_gpu_source,flavor.stabilize_animated_direct);}
        if (flavor.native_resources && animated_admitted) publishResourceSource(*value, variant.prototype);
        if (flavor.cohort_source)
            sealCohortSource(*value, variant, info.metalSourcePublication()->identity(), value->producer_revision);
        if (flavor.animated_source && animated_admitted)
            sealAnimatedSource(*value,variant,info.metalSourcePublication()->identity(),value->producer_revision,flavor.animated_gpu_source);
        if (!publish(info, value, working_set, frame))
            return {{}, nullptr, "Native scene producer changed while sealing"};
        if(animated_ticket && animatedSourceReady(*value,variant,flavor.animated_gpu_source))animated_ticket->publicationCompleted();
        const auto* prototype = &value->worlds.back().prototype;
        return {std::move(value), prototype};
    }
    catch (const std::bad_alloc&)
    {
        return {{}, nullptr, "Native scene publication allocation failed"};
    }
}

inline std::shared_ptr<const ALMetalTexture> matchImage(
    const LLDrawInfo& info, const ALMetalTexture* requested)
{
    std::shared_ptr<const ALMetalTexture> result;
    const auto image = [&](LLViewerTexture* texture)
    {
        if (result || !texture) return;
        auto* native = texture->getGLTexture();
        if (native && native->getMetalTexture() == requested) result = native->getMetalTextureLease();
    };
    const auto material = [&](LLFetchedGLTFMaterial* value)
    {
        if (!value) return;
        for (U32 slot = 0; slot < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++slot)
            image(value->getRenderTexture(static_cast<LLGLTFMaterial::TextureInfo>(slot)));
    };
    image(info.mTexture.get()); image(info.mSpecularMap.get()); image(info.mNormalMap.get());
    for (const auto& value : info.mTextureList) image(value.get());
    for (const auto& value : info.mMaterialSlotList)
    { image(value.mDiffuse.get()); image(value.mNormalMap.get()); image(value.mSpecularMap.get()); }
    material(info.mGLTFMaterial.get());
    for (const auto& value : info.mGLTFMaterialList) material(value.get());
    image(LLViewerFetchedTexture::sDefaultImagep.get());
    image(LLViewerFetchedTexture::sWhiteImagep.get());
    image(LLViewerFetchedTexture::sFlatNormalImagep.get());
    return result;
}

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
}
