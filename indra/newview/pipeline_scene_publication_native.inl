/**
 * @file pipeline_scene_publication_native.inl
 * @brief Owner-sealed native object sources shared by main/probe/shadow views.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Include after ALViewerMetalShadowSourceCache is defined. This file contains
 * no live scene access on workers and no payload-destructor owner mutations.
 */
#include "alnativesourceworkingset.h"
#include "almetalshadowprepare.h"
#include "almetalworldvisibility.h"
#include "almetalworldmaterialdata.h"
#include "almetalworldresources.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>
#include "../almetal/almetalworldcohort.h"

namespace ALScenePublicationNative
{
enum class WorldFamily : U8 { simple, legacy, pbr };
inline std::unique_ptr<ALMetalWorldMaterialDataCache> material_data_cache;
inline U64 material_source_publications = 0, material_source_fallbacks = 0;
inline U64 resource_source_publications = 0, resource_source_fallbacks = 0;
inline U64 cohort_source_publications = 0;
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
    bool normal_map = false, specular_map = false;
    bool alpha_mask = false, rigged = false, pbr_glow = false;
    bool impostor_fullbright = false, impostor_shiny = false, impostor_emboss = false;
    U32 material_alpha_mode = 0;
    bool hasResourceConsumer() const noexcept
    {
        return native_materials && family != WorldFamily::simple &&
            !(family == WorldFamily::legacy && material_alpha_mode == 1) &&
            !pbr_glow && !impostor_fullbright && !impostor_shiny && !impostor_emboss;
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
            (U64(material_alpha_mode) << 32);
    }
};

struct WorldVariant
{
    U64 key = 0;
    ALMetalWorldSimpleBatch prototype;
    std::shared_ptr<const ALMetalWorldCohortSource> cohort_source;
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
        return std::static_pointer_cast<const Payload>(metadata->native_scene_transient.lock());
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
inline void qualifyBounds(const LLDrawInfo& info, Payload& payload)
{
    if (payload.bounds.qualified) return;
    auto* value = &payload;
    // Exact published rigid position plane, in the same local coordinates
    // consumed by the vertex shader. Unproved/animated extents fail visible.
    auto* buffer = info.mVertexBuffer.get();
    const U8* bytes = buffer->getMappedData();
    if (!bytes) bytes = static_cast<const U8*>(value->vertices->mappedBytes());
    const U8* indices = buffer->getMappedIndices();
    if (!indices) indices = static_cast<const U8*>(value->indices->mappedBytes());
    const auto offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
    if (bytes && indices && !info.mAvatar &&
        offset + std::size_t(buffer->getNumVerts()) * 16 <= value->vertices->length())
    {
        bool finite = true;
        for (U32 axis = 0; axis < 3; ++axis)
        {
            value->bounds.minimum[axis] = std::numeric_limits<float>::infinity();
            value->bounds.maximum[axis] = -std::numeric_limits<float>::infinity();
        }
        for (U32 at = 0; at < info.mCount; ++at)
        {
            U32 vertex = 0;
            const auto* index = indices + std::size_t(info.mOffset + at) * buffer->getIndexStride();
            if (buffer->getIndexStride() == 2)
            { U16 small; std::memcpy(&small, index, sizeof(small)); vertex = small; }
            else std::memcpy(&vertex, index, sizeof(vertex));
            if (vertex >= buffer->getNumVerts()) { finite = false; break; }
            float point[4]; std::memcpy(point, bytes + offset + std::size_t(vertex) * 16, sizeof(point));
            for (U32 axis = 0; axis < 3; ++axis)
            {
                finite = finite && std::isfinite(point[axis]);
                value->bounds.minimum[axis] = std::min(value->bounds.minimum[axis], point[axis]);
                value->bounds.maximum[axis] = std::max(value->bounds.maximum[axis], point[axis]);
            }
        }
        value->bounds.qualified = finite;
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
        batch.diffuse_textures[slot] = resolve(value, base ? base : white());
        destination.normal_texture = resolve(value, normal ? normal : flat());
        destination.orm_texture = resolve(value, orm ? orm : white());
        destination.emissive_texture = resolve(value, emissive ? emissive : white());
        if (!batch.diffuse_textures[slot] || !destination.normal_texture ||
            !destination.orm_texture || !destination.emissive_texture) return false;
    }
    return true;
}

inline void publishResourceSource(Payload& value, ALMetalWorldSimpleBatch& prototype)
{
    // Seal exact generation leases at the same dirty-source boundary.
    // A native texture retain alone would not participate in LLImageGL
    // reader/COW protection. Resolve only the already-accounted tuple;
    // no later view or worker rereads a mutable viewer texture facade.
    try
    {
        std::vector<std::shared_ptr<const ALMetalTexture>> images;
        const auto& batch = static_cast<const ALMetalWorldSimpleBatch&>(prototype);
        images.reserve(batch.texture_count * 4);
        const auto image = [&](const ALMetalTexture* texture)
        {
            if (!texture) return;
            for (const auto& existing : images)
                if (existing.get() == texture) return;
            for (const auto& resource : value.resources)
                if (resource.allocation.get() == texture)
                {
                    images.emplace_back(resource.allocation, texture);
                    return;
                }
        };
        for (U32 slot = 0; slot < batch.texture_count; ++slot)
            image(batch.diffuse_textures[slot]);
        for (U32 slot = 0; slot < batch.material_slot_count; ++slot)
        {
            image(batch.materialSlots()[slot].normal_texture);
            image(batch.materialSlots()[slot].specular_texture);
        }
        for (U32 slot = 0; slot < batch.pbr_slot_count; ++slot)
        {
            image(batch.pbrSlots()[slot].normal_texture);
            image(batch.pbrSlots()[slot].orm_texture);
            image(batch.pbrSlots()[slot].emissive_texture);
        }
        auto* device = LLImageGL::getMetalDevice();
        auto failure = ALMetalWorldResourceFallback::unsupported;
        if (device && ALMetalWorldResourceSource::publish(
                *device, prototype, images, nullptr, &failure))
            ++resource_source_publications;
        else resourceSourceFailed(failure);
    }
    catch (const std::bad_alloc&) { resourceSourceFailed(ALMetalWorldResourceFallback::host_allocation); }
}
inline void sealCohortSource(WorldVariant& variant, U64 identity, U64 revision)
{
    if (variant.cohort_source) return;
    const auto& batch = variant.prototype;
    if (!batch.native_resource_source || !batch.native_resource_source->matches(batch)) return;
    variant.cohort_source = ALMetalWorldCohortSource::seal({{identity, variant.key, revision}, &batch});
    if (variant.cohort_source) ++cohort_source_publications;
}

inline WorldLease world(const LLDrawInfo& info, WorldFlavor flavor,
                        ALNativeSourceWorkingSet& working_set, U64& serial, U32 frame)
{
    try
    {
        info.observeMetalSourceDependencies();
        if (flavor.family == WorldFamily::simple)
            flavor.indexed_textures = flavor.indexed_textures && info.mTextureList.size() > 1;
        flavor.native_resources = flavor.native_resources && flavor.hasResourceConsumer();
        const U64 key = flavor.key();
        if (auto value = current(info))
            if (const auto* found = findWorldVariant(*value, key))
            {
                const auto ready = [&](const WorldVariant& variant)
                {
                    return (!flavor.native_resources || (variant.prototype.native_resource_source &&
                        variant.prototype.native_resource_source->matches(variant.prototype))) &&
                        (!flavor.cohort_source || variant.cohort_source);
                };
                if (ready(*found))
                {
                    working_set.touch(info.metalSourcePublication()->identity(), value->serial, frame);
                    return {std::move(value), &found->prototype, nullptr, true};
                }
                // Resource/cohort capabilities do not change shader semantics.
                // Promote a completed immutable variant from its exact native
                // tuple, without rebuilding LL geometry/material/texture data.
                const auto state = info.metalSourcePublication();
                const auto identity = state->identity();
                const auto revision = value->producer_revision;
                const auto original_current = [&]
                { return state->identity() == identity && state->revision() == revision && state->publication().get() == value.get(); };
                try
                {
                    if (capabilityPromotionAllocationForTesting) capabilityPromotionAllocationForTesting(*state);
                    const auto index = static_cast<std::size_t>(found - value->worlds.data());
                    auto promoted = std::make_shared<Payload>(*value);
                    auto& variant = promoted->worlds[index];
                    if (flavor.native_resources && (!variant.prototype.native_resource_source ||
                        !variant.prototype.native_resource_source->matches(variant.prototype)))
                        publishResourceSource(*promoted, variant.prototype);
                    if (flavor.cohort_source)
                        sealCohortSource(variant, info.metalSourcePublication()->identity(), promoted->producer_revision);
                    if (!ready(variant))
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
                    const auto* prototype = &promoted->worlds[index].prototype;
                    return {std::move(promoted), prototype, nullptr, true};
                }
                catch (const std::bad_alloc&)
                {
                    if (original_current())
                        return {std::move(value), &found->prototype, nullptr, true};
                    return {{}, nullptr, "Native scene changed while optional promotion allocation failed"};
                }
            }
        if (!geometry(info)) return {{}, nullptr, "Native scene geometry is unpublished or invalid"};
        info.observeMetalSourceDependencies();
        auto value = extend(info, serial);
        if (!value) return {{}, nullptr, "Native scene geometry lease is missing"};
        if (flavor.visibility_bounds) qualifyBounds(info, *value);
        auto& variant = value->worlds.emplace_back();
        variant.key = key;
        if (!buildWorld(info, flavor, *value, variant.prototype))
            return {{}, nullptr, "Native scene world material/layout is incomplete"};
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
        if (flavor.native_resources) publishResourceSource(*value, variant.prototype);
        if (flavor.cohort_source)
            sealCohortSource(variant, info.metalSourcePublication()->identity(), value->producer_revision);
        if (!publish(info, value, working_set, frame))
            return {{}, nullptr, "Native scene producer changed while sealing"};
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
            if (const auto* record = findShadow(*value, type, coverage))
            {
                working_set.touch(info.metalSourcePublication()->identity(), value->serial, frame);
                return {std::move(value), record, nullptr, true};
            }
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
