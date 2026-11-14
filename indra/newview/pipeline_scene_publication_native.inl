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
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace ALScenePublicationNative
{
enum class WorldFamily : U8 { simple, legacy, pbr };
inline std::unique_ptr<ALMetalWorldMaterialDataCache> material_data_cache;
inline U64 material_source_publications = 0, material_source_fallbacks = 0;
inline void clearMaterialDataCache() noexcept { material_data_cache.reset(); }
inline ALMetalWorldMaterialDataCache::Stats materialDataStats() noexcept
{ return material_data_cache ? material_data_cache->stats() : ALMetalWorldMaterialDataCache::Stats{}; }


struct WorldFlavor
{
    WorldFamily family = WorldFamily::simple;
    bool indexed_textures = false, indexed_materials = false;
    bool visibility_bounds = false;
    bool native_materials = true;
    bool normal_map = false, specular_map = false;
    bool alpha_mask = false, rigged = false, pbr_glow = false;
    bool impostor_fullbright = false, impostor_shiny = false, impostor_emboss = false;
    U32 material_alpha_mode = 0;
    U64 key() const noexcept
    {
        return static_cast<U64>(family) | (U64(indexed_textures) << 2) |
            (U64(indexed_materials) << 3) | (U64(normal_map) << 4) |
            (U64(specular_map) << 5) | (U64(alpha_mask) << 6) |
            (U64(rigged) << 7) | (U64(pbr_glow) << 8) |
            (U64(impostor_fullbright) << 9) | (U64(impostor_shiny) << 10) |
            (U64(impostor_emboss) << 11) | (U64(visibility_bounds) << 12) | (U64(native_materials && family != WorldFamily::simple) << 13) | (U64(material_alpha_mode) << 32);
    }
};

struct WorldVariant
{
    U64 key = 0;
    ALMetalWorldSimpleBatch prototype;
};
struct ShadowVariant
{
    U32 type = 0;
    ALMetalWorldSpotShadowCoverage coverage = ALMetalWorldSpotShadowCoverage::opaque;
    std::shared_ptr<const ALMetalShadowSourceRecord> record;
};
struct Payload
{
    U64 serial = 0, producer_revision = 0;
    ALMetalWorldConservativeBounds bounds;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    std::vector<WorldVariant> worlds;
    std::vector<ShadowVariant> shadows;
    std::vector<ALNativeSourceWorkingSet::Resource> resources;
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
    if (!state->publish(value->producer_revision, value)) return false;
    auto& metadata = *info.mMetalShadowSource;
    metadata.native_scene_revision = value->producer_revision;
    metadata.native_scene_transient = value;
    const std::shared_ptr<const void> expected = value;
    // The caller already owns value. Rejection/eviction drops only State's
    // publisher lease; actual packets may still reuse the weak transient.
    working_set.retain(state, value->serial, expected, value->resources, frame);
    return true;
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

inline WorldLease world(const LLDrawInfo& info, WorldFlavor flavor,
                        ALNativeSourceWorkingSet& working_set, U64& serial, U32 frame)
{
    try
    {
        info.observeMetalSourceDependencies();
        if (flavor.family == WorldFamily::simple)
            flavor.indexed_textures = flavor.indexed_textures && info.mTextureList.size() > 1;
        const U64 key = flavor.key();
        if (auto value = current(info))
            if (const auto* prototype = findWorld(*value, key))
            {
                working_set.touch(info.metalSourcePublication()->identity(), value->serial, frame);
                return {std::move(value), prototype, nullptr, true};
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
