/**
 * @file almetalshadowmaterials.mm
 * @brief Completion-safe native shadow material tables.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalshadowmaterials.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
void setError(std::string* error, const char* message)
{
    if (error) *error = message;
}

struct Key
{
    std::uint32_t slots = 0;
    std::array<void*, 32> views{};
    void* sampler = nullptr;
    bool operator==(const Key&) const = default;
};

struct KeyHash
{
    std::size_t operator()(const Key& key) const noexcept
    {
        std::size_t h = key.slots;
        const auto mix = [&h](void* p)
        {
            const auto value = std::hash<void*>{}(p);
            h ^= value + 0x9e3779b9u + (h << 6) + (h >> 2);
        };
        mix(key.sampler);
        for (std::size_t i = 0; i < key.slots; ++i) mix(key.views[i]);
        return h;
    }
};

struct Slot
{
    std::unique_ptr<ALMetalBuffer> buffer;
    std::size_t charged_bytes = 0;
};

struct Entry
{
    Key key;
    std::shared_ptr<Slot> slot;
    // Exact shader-view identities are strongly held while cached and while
    // any command owns this immutable Entry. Thus raw-address ABA cannot hit.
    std::array<id<MTLTexture>, 32> textures{};
    id<MTLSamplerState> sampler = nil;
    std::uint64_t last_generation = 0;
};

struct Pool
{
    const ALMetalDevice* device_wrapper = nullptr;
    id<MTLDevice> device = nil;
    std::array<id<MTLFunction>, 2> functions{};
    std::array<std::size_t, 2> encoded_lengths{};
    std::array<std::size_t, 2> alignments{};
    ALMetalShadowMaterialLimits limits;
    bool supported = false;
    bool initialized = false;
    std::uint64_t generation = 0;
    std::vector<std::shared_ptr<Slot>> slots;
    std::size_t allocated_bytes = 0;
    std::unordered_map<Key, std::shared_ptr<Entry>, KeyHash> cache;
    ALMetalShadowMaterialStats statistics;
};

// Crucially owns no command, completion token, renderer or Pool. The existing
// command completion callback clears this owner; abandonment releases it too.
struct CommandOwner
{
    std::vector<std::shared_ptr<Entry>> entries;
    std::vector<id<MTLResource>> resources;
};

struct Snapshot
{
    Key key;
    std::array<id<MTLTexture>, 32> textures{};
    id<MTLSamplerState> sampler = nil;
};

bool requestKey(const Pool& pool, const ALMetalShadowMaterialRequest& request,
                Key& key, std::string* error)
{
    if (!request.textures || request.texture_count == 0 ||
        request.texture_count > 32 || !request.sampler)
    {
        setError(error, "Metal shadow material request is incomplete");
        return false;
    }
    key = {};
    key.sampler = request.sampler->nativeHandle();
    if (!key.sampler || (pool.supported && request.texture_count > 1 &&
                         !request.sampler->supportsArgumentBuffers()))
    {
        setError(error, "Metal shadow material sampler is invalid or not argument-buffer capable");
        return false;
    }
    key.slots = request.texture_count == 1 ? 1 :
        (request.texture_count <= 8 ? 8 : 32);
    for (std::size_t i = 0; i < key.slots; ++i)
    {
        const auto* source = request.textures[std::min<std::size_t>(
            i, request.texture_count - 1)];
        if (!source || source->descriptor().type != ALMetalTextureType::texture_2d ||
            source->descriptor().sample_count != 1 ||
            !alMetalHasUsage(source->descriptor().usage, ALMetalTextureUsage::shader_read) ||
            !(key.views[i] = source->nativeHandle()))
        {
            setError(error, "Metal shadow material texture view is invalid");
            return false;
        }
    }
    return true;
}

bool captureSnapshot(const Pool& pool, const Key& key,
                     Snapshot& snapshot, std::string* error)
{
    snapshot = {};
    snapshot.key = key;
    snapshot.sampler = (__bridge id<MTLSamplerState>)key.sampler;
    // Only unique cache misses reach native property validation/strong ARC
    // promotion. Cached Entries already own these immutable validated objects.
    // Use the same captured IDs throughout, without rereading mutable wrappers.
    for (std::size_t i = 0; i < key.slots; ++i)
        snapshot.textures[i] = (__bridge id<MTLTexture>)key.views[i];
    if (snapshot.sampler.device != pool.device ||
        (pool.supported && key.slots > 1 && snapshot.sampler.gpuResourceID._impl == 0))
    {
        setError(error, "Metal shadow material sampler belongs to another device or has no GPU ID");
        return false;
    }
    for (std::size_t i = 0; i < key.slots; ++i)
    {
        const auto texture = snapshot.textures[i];
        if (texture.device != pool.device || texture.textureType != MTLTextureType2D ||
            texture.sampleCount != 1)
        {
            setError(error, "Metal shadow material native texture view is invalid");
            return false;
        }
    }
    return true;
}

bool validateWithoutCache(const Pool& pool,
                          std::span<const ALMetalShadowMaterialRequest> requests,
                          std::string* error)
{
    for (const auto& request : requests)
    {
        Key key;
        Snapshot snapshot;
        if (!requestKey(pool, request, key, error) ||
            !captureSnapshot(pool, key, snapshot, error)) return false;
    }
    return true;
}

struct PreparedMaterial
{
    explicit PreparedMaterial(const Key& input) : key(input) {}
    Key key;
    std::shared_ptr<Entry> entry;
    std::size_t references = 0;
};

// Pass-local exact-key registry: two contiguous allocations, not one heap
// node per unique material. Reserved records keep request pointers stable.
// Hashes choose a probe position only; complete Key equality decides reuse.
struct PreparedMaterials
{
    static constexpr std::size_t no_index = std::numeric_limits<std::size_t>::max();
    std::vector<PreparedMaterial> entries;
    std::vector<std::size_t> buckets;
    std::size_t maximum_entries;

    explicit PreparedMaterials(std::size_t maximum) : maximum_entries(maximum)
    {
        if (maximum > no_index / 2) throw std::bad_alloc();
        const std::size_t required = maximum * 2;
        std::size_t capacity = 8;
        while (capacity < required)
        {
            if (capacity > no_index / 2) throw std::bad_alloc();
            capacity *= 2;
        }
        entries.reserve(maximum);
        buckets.assign(capacity, no_index);
    }

    std::size_t bucketFor(const Key& key) const noexcept
    {
        // Native handles are aligned. Avalanche the existing key hash before
        // a power-of-two mask so last-slot-only changes do not share low bits.
        std::uint64_t hash = KeyHash{}(key);
        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ull;
        hash ^= hash >> 27;
        hash *= 0x94d049bb133111ebull;
        hash ^= hash >> 31;
        return static_cast<std::size_t>(hash) & (buckets.size() - 1);
    }

    std::pair<PreparedMaterial*, bool> findOrInsert(const Key& key)
    {
        const std::size_t mask = buckets.size() - 1;
        std::size_t position = bucketFor(key);
        while (buckets[position] != no_index)
        {
            auto& candidate = entries[buckets[position]];
            if (candidate.key == key) return {&candidate, false};
            position = (position + 1) & mask;
        }
        if (entries.size() == maximum_entries) throw std::bad_alloc();
        entries.emplace_back(key);
        buckets[position] = entries.size() - 1;
        return {&entries.back(), true};
    }
};

// One strong native snapshot per distinct object in this preparation, rather
// than32 ARC copies/property queries for every new material combination. All
// fallback resources are also validated before ANY cache mutation.
struct NativeValidation
{
    struct Sampler
    {
        id<MTLSamplerState> native = nil;
        bool argument_id_checked = false;
    };
    std::unordered_map<void*, id<MTLTexture>> textures;
    std::unordered_map<void*, Sampler> samplers;

    bool include(const Pool& pool, const Key& key, std::string* error)
    {
        auto sampler = samplers.find(key.sampler);
        if (sampler == samplers.end())
        {
            // Promote the captured ID before property access or memoization.
            id<MTLSamplerState> native = (__bridge id<MTLSamplerState>)key.sampler;
            if (native.device != pool.device)
            {
                setError(error, "Metal shadow material sampler belongs to another device");
                return false;
            }
            Sampler value;
            value.native = native;
            sampler = samplers.emplace(key.sampler, std::move(value)).first;
        }
        if (pool.supported && key.slots > 1 && !sampler->second.argument_id_checked)
        {
            if (sampler->second.native.gpuResourceID._impl == 0)
            {
                setError(error, "Metal shadow material sampler has no GPU ID");
                return false;
            }
            sampler->second.argument_id_checked = true;
        }
        for (std::size_t i = 0; i < key.slots; ++i)
        {
            if (textures.find(key.views[i]) != textures.end()) continue;
            id<MTLTexture> native = (__bridge id<MTLTexture>)key.views[i];
            if (native.device != pool.device || native.textureType != MTLTextureType2D ||
                native.sampleCount != 1)
            {
                setError(error, "Metal shadow material native texture view is invalid");
                return false;
            }
            textures.emplace(key.views[i], native);
        }
        return true;
    }
};

enum class MissResult { ready, capacity, allocation, invalid };

MissResult createEntry(Pool& pool, const Key& key,
                       const NativeValidation& validated,
                       std::array<id<MTLArgumentEncoder>, 2>& encoders,
                       std::shared_ptr<Entry>& result, std::string* error)
{
    @autoreleasepool
    {
        const std::size_t family = key.slots == 8 ? 0 : 1;
        const auto length = pool.encoded_lengths[family];
        std::shared_ptr<Slot> slot;
        // Each cached Entry leases a distinct slot. Equality therefore proves
        // that none can be free, avoiding a full leased-slot scan per miss.
        const bool all_slots_cached = pool.cache.size() == pool.slots.size();
        if (!all_slots_cached)
        {
            for (const auto& candidate : pool.slots)
            {
                ++pool.statistics.slot_scan_visits;
                // Pool is the only owner exactly when no cached Entry, CPU pass
                // or pending command can still read these bytes.
                if (candidate.use_count() == 1 && candidate->buffer->length() >= length &&
                    (!slot || candidate->buffer->length() < slot->buffer->length()))
                    slot = candidate;
            }
        }
        if (slot) ++pool.statistics.slot_reuses;
        else
        {
            std::shared_ptr<Slot> replace;
            if (!all_slots_cached)
            {
                for (const auto& candidate : pool.slots)
                {
                    ++pool.statistics.slot_scan_visits;
                    if (candidate.use_count() == 1) { replace = candidate; break; }
                }
            }
            const std::size_t old_charge = replace ? replace->charged_bytes : 0;
            if ((!replace && pool.slots.size() >= pool.limits.maximum_slots) ||
                length > pool.limits.maximum_buffer_bytes ||
                pool.allocated_bytes - old_charge > pool.limits.maximum_buffer_bytes - length)
                return MissResult::capacity;
            ALMetalBufferDescriptor descriptor;
            descriptor.length = length;
            descriptor.storage = ALMetalStorageMode::shared;
            descriptor.label = "Alchemy immutable covered shadow material";
            std::string allocation_error;
            auto buffer = ALMetalBuffer::create(*pool.device_wrapper, descriptor,
                                                &allocation_error);
            if (!buffer) return MissResult::allocation;
            const auto native_buffer = (__bridge id<MTLBuffer>)buffer->nativeHandle();
            const std::size_t charge = std::max<std::size_t>(
                buffer->length(), native_buffer.allocatedSize);
            if (charge > pool.limits.maximum_buffer_bytes ||
                pool.allocated_bytes - old_charge > pool.limits.maximum_buffer_bytes - charge)
                return MissResult::capacity;
            if (replace)
            {
                slot = std::move(replace);
                slot->buffer = std::move(buffer);
            }
            else
            {
                slot = std::make_shared<Slot>();
                slot->buffer = std::move(buffer);
                pool.slots.push_back(slot);
            }
            slot->charged_bytes = charge;
            pool.allocated_bytes = pool.allocated_bytes - old_charge + charge;
        }
        // Admission comes first: capacity/allocation fallback never creates
        // an argument encoder. Each family creates at most one per prepare(),
        // not one per material; no encoder persists across views or frames.
        if (!encoders[family])
        {
            ++pool.statistics.argument_encoder_creations;
            id<MTLArgumentEncoder> encoder = [pool.functions[family]
                newArgumentEncoderWithBufferIndex:1];
            if (!encoder || encoder.encodedLength != pool.encoded_lengths[family] ||
                encoder.alignment != pool.alignments[family])
            {
                setError(error, "Metal shadow material argument-buffer ABI changed");
                return MissResult::invalid;
            }
            encoders[family] = encoder;
        }
        id<MTLArgumentEncoder> encoder = encoders[family];
        auto entry = std::make_shared<Entry>();
        entry->key = key;
        entry->slot = std::move(slot);
        // Only admitted tables create a per-material strong resource array.
        // These are the exact IDs pinned/validated earlier, never reread from
        // mutable image wrappers after admission.
        for (std::size_t i = 0; i < key.slots; ++i)
        {
            const auto found = validated.textures.find(key.views[i]);
            if (found == validated.textures.end())
            {
                setError(error, "Metal shadow material texture validation is missing");
                return MissResult::invalid;
            }
            entry->textures[i] = found->second;
        }
        const auto sampler = validated.samplers.find(key.sampler);
        if (sampler == validated.samplers.end())
        {
            setError(error, "Metal shadow material sampler validation is missing");
            return MissResult::invalid;
        }
        entry->sampler = sampler->second.native;
        entry->last_generation = pool.generation;
        [encoder setArgumentBuffer:(__bridge id<MTLBuffer>)entry->slot->buffer->nativeHandle()
                            offset:0];
        for (std::size_t i = 0; i < key.slots; ++i)
            [encoder setTexture:entry->textures[i] atIndex:i];
        [encoder setSamplerState:entry->sampler atIndex:key.slots];
        // No setters ever change this table until every Entry lease has gone.
        [encoder setArgumentBuffer:nil offset:0];
        pool.cache.emplace(entry->key, entry);
        ++pool.statistics.table_creates;
        result = std::move(entry);
        return MissResult::ready;
    }
}
}

struct ALMetalShadowMaterials::Impl
{
    std::shared_ptr<Pool> pool;
};

struct ALMetalShadowMaterials::Pass::Impl
{
    std::shared_ptr<Pool> pool;
    std::shared_ptr<CommandOwner> owner;
    std::vector<Entry*> requests;
    std::size_t request_count = 0;
    void* command = nullptr; // Identity only. Never retained from its own owner.
    // CPU-only strong identity prevents encoder-address reuse during this
    // Pass. CommandOwner does not retain Pass, so no completion cycle results.
    id<MTLRenderCommandEncoder> declared_encoder = nil;
};

ALMetalShadowMaterials::ALMetalShadowMaterials(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalShadowMaterials::~ALMetalShadowMaterials() = default;
ALMetalShadowMaterials::Pass::Pass(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}
ALMetalShadowMaterials::Pass::~Pass() = default;

std::unique_ptr<ALMetalShadowMaterials> ALMetalShadowMaterials::create(
    const ALMetalDevice& device, const ALMetalShaderLibrary& library,
    std::string* error, ALMetalShadowMaterialLimits limits)
{
    @autoreleasepool
    {
        if (!device.nativeHandle() || !library.nativeHandle())
        {
            setError(error, "Metal shadow materials require a device and library");
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->pool = std::make_shared<Pool>();
        auto& pool = *impl->pool;
        pool.device_wrapper = &device;
        pool.device = (__bridge id<MTLDevice>)device.nativeHandle();
        pool.limits = limits;
        id<MTLLibrary> native_library = (__bridge id<MTLLibrary>)library.nativeHandle();
        if (native_library.device != pool.device)
        {
            setError(error, "Metal shadow material library belongs to another device");
            return nullptr;
        }
        pool.supported = pool.device.argumentBuffersSupport == MTLArgumentBuffersTier2;
        if (pool.supported)
        {
            constexpr std::array<const char*, 2> names{{
                "almetal_world_spot_shadow_coverage_eight_material_fragment",
                "almetal_world_spot_shadow_coverage_material_fragment"}};
            for (std::size_t i = 0; i < names.size(); ++i)
            {
                pool.functions[i] = [native_library newFunctionWithName:
                    [NSString stringWithUTF8String:names[i]]];
                id<MTLArgumentEncoder> encoder = [pool.functions[i]
                    newArgumentEncoderWithBufferIndex:1];
                if (!pool.functions[i] || !encoder || encoder.encodedLength == 0 ||
                    encoder.alignment == 0)
                {
                    setError(error, "Metal shadow material shader ABI is unavailable");
                    return nullptr;
                }
                pool.encoded_lengths[i] = encoder.encodedLength;
                pool.alignments[i] = encoder.alignment;
            }
        }
        return std::unique_ptr<ALMetalShadowMaterials>(
            new ALMetalShadowMaterials(std::move(impl)));
    }
}

bool ALMetalShadowMaterials::supported() const noexcept
{
    return mImpl && mImpl->pool->supported;
}

void ALMetalShadowMaterials::beginFrame(std::uint64_t generation)
{
    auto& pool = *mImpl->pool;
    if (pool.initialized && pool.generation == generation) return;
    if (pool.initialized)
    {
        for (auto i = pool.cache.begin(); i != pool.cache.end(); )
        {
            if (i->second->last_generation != pool.generation) i = pool.cache.erase(i);
            else ++i;
        }
    }
    pool.initialized = true;
    pool.generation = generation;
}

std::unique_ptr<ALMetalShadowMaterials::Pass> ALMetalShadowMaterials::prepare(
    ALMetalCommandBuffer& command,
    std::span<const ALMetalShadowMaterialRequest> requests,
    std::string* error)
{
    auto& pool = *mImpl->pool;
    if (!command.nativeHandle() || command.isCommitted() ||
        command.nativeDevice() != (__bridge void*)pool.device)
    {
        setError(error, "Metal shadow material preparation requires its uncommitted command");
        return nullptr;
    }
    auto pass_impl = std::make_unique<Pass::Impl>();
    pass_impl->pool = mImpl->pool;
    pass_impl->request_count = requests.size();
    pass_impl->command = command.nativeHandle();
    auto pass = std::unique_ptr<Pass>(new Pass(std::move(pass_impl)));
    if (!pool.supported)
    {
        if (!validateWithoutCache(pool, requests, error)) return nullptr;
        ++pool.statistics.prepared_passes;
        pool.statistics.direct_requests += requests.size();
        return pass;
    }
    bool counted_pass = false;
    try
    {
        std::uint64_t direct_count = 0;
        auto owner = std::make_shared<CommandOwner>();
        const std::size_t maximum_tables = std::min(requests.size(), pool.limits.maximum_slots);
        owner->entries.reserve(maximum_tables);
        pass->mImpl->requests.assign(requests.size(), nullptr);
        std::vector<PreparedMaterial*> request_materials(requests.size(), nullptr);
        PreparedMaterials unique(requests.size());
        NativeValidation validated;
        std::unordered_set<void*> seen_resources;
        // Validate EVERY logical request before cache mutation or retention.
        // Duplicate requests do CPU pointer/descriptor checks + key hashing;
        // native validation/ARC promotion happen once per unique native object,
        // including resources belonging only to requests that will bind direct.
        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            Key key;
            if (!requestKey(pool, requests[index], key, error)) return nullptr;
            auto [material, inserted] = unique.findOrInsert(key);
            if (inserted)
            {
                const auto found = pool.cache.find(key);
                if (found != pool.cache.end())
                {
                    material->entry = found->second;
                }
                else if (!validated.include(pool, key, error)) return nullptr;
            }
            ++material->references;
            request_materials[index] = material;
        }
        std::size_t new_materials = 0;
        bool all_new_single_use = true;
        for (const auto& material : unique.entries)
        {
            if (material.entry || material.key.slots == 1) continue;
            ++new_materials;
            all_new_single_use = all_new_single_use && material.references == 1;
        }
        // A one-use working set that cannot fit should not build a cache-sized
        // prefix of tables only to draw the rest direct. This changes resource
        // admission only; cached hits remain usable and every draw is retained.
        const bool direct_new_materials = all_new_single_use &&
            new_materials > pool.limits.maximum_slots;
        if (!pool.initialized) beginFrame(0);
        ++pool.statistics.prepared_passes;
        counted_pass = true;
        // Whole-input validity is now established. Resolve unique tables once.
        std::array<id<MTLArgumentEncoder>, 2> encoders{};
        std::array<MissResult, 2> miss_failure{
            MissResult::ready, MissResult::ready};
        for (auto& material : unique.entries)
        {
            const auto& key = material.key;
            if (key.slots == 1) continue;
            if (material.entry)
            {
                material.entry->last_generation = pool.generation;
                pool.statistics.cache_hits += material.references;
            }
            else
            {
                ++pool.statistics.cache_misses;
                const std::size_t family = key.slots == 8 ? 0 : 1;
                const auto result = direct_new_materials ? MissResult::capacity :
                    (miss_failure[family] != MissResult::ready
                        ? miss_failure[family]
                        : createEntry(pool, key, validated, encoders, material.entry, error));
                if (result == MissResult::invalid) return nullptr;
                if (result != MissResult::ready)
                {
                    // Do not rescan/reallocate for thousands of fresh keys
                    // after a family has failed optional admission this pass.
                    // Existing hits above remain usable. Reclaim completing
                    // GPU leases on the next prepare(), without blocking here.
                    miss_failure[family] = result;
                    if (result == MissResult::capacity) ++pool.statistics.capacity_fallbacks;
                    else ++pool.statistics.allocation_fallbacks;
                    continue;
                }
                pool.statistics.cache_hits += material.references - 1;
            }
            owner->entries.push_back(material.entry);
            for (std::size_t i = 0; i < key.slots; ++i)
                if (seen_resources.insert(key.views[i]).second)
                    owner->resources.push_back(material.entry->textures[i]);
        }
        for (std::size_t index = 0; index < requests.size(); ++index)
        {
            auto* entry = request_materials[index]->entry.get();
            pass->mImpl->requests[index] = entry;
            if (!entry) ++direct_count;
        }
        if (!owner->entries.empty())
        {
            // Exactly one owner registration per prepared pass, before ANY
            // encoder can consume a table. Owners survive helper destruction.
            if (!command.retainUntilCompletion(owner, error)) return nullptr;
            pass->mImpl->owner = std::move(owner);
        }
        pool.statistics.direct_requests += direct_count;
    }
    catch (const std::bad_alloc&)
    {
        // Optional CPU cache growth must not remove a valid direct draw. No
        // owner has been registered until the final nonallocating assignment.
        pass->mImpl->requests.clear();
        pass->mImpl->owner.reset();
        // Even optional preflight-cache allocation failure cannot conceal a
        // later invalid source. This slow path uses only bounded stack state.
        if (!validateWithoutCache(pool, requests, error)) return nullptr;
        if (!counted_pass) ++pool.statistics.prepared_passes;
        pool.statistics.direct_requests += requests.size();
        ++pool.statistics.allocation_fallbacks;
    }
    return pass;
}

std::size_t ALMetalShadowMaterials::Pass::requestCount() const noexcept
{
    return mImpl->request_count;
}

bool ALMetalShadowMaterials::Pass::usesArgumentBuffer(std::size_t request) const noexcept
{
    return request < mImpl->requests.size() && mImpl->requests[request] != nullptr;
}

bool ALMetalShadowMaterials::Pass::declareResources(
    ALMetalRenderEncoder& encoder, std::string* error)
{
    if (!encoder.nativeHandleForIndirect() ||
        encoder.nativeDeviceForIndirect() != (__bridge void*)mImpl->pool->device ||
        encoder.nativeCommandBufferForIndirect() != mImpl->command)
    {
        setError(error, "Metal shadow material pass requires its original active command encoder");
        return false;
    }
    id<MTLRenderCommandEncoder> native = (__bridge id<MTLRenderCommandEncoder>)
        encoder.nativeHandleForIndirect();
    if (mImpl->declared_encoder)
    {
        if (mImpl->declared_encoder == native) return true;
        setError(error, "Metal shadow material pass is already declared on another encoder");
        return false;
    }
    if (mImpl->owner && !mImpl->owner->resources.empty())
    {
        const auto* resources = reinterpret_cast<
            id<MTLResource> __unsafe_unretained const*>(
                mImpl->owner->resources.data());
        [native useResources:resources
                       count:mImpl->owner->resources.size()
                       usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        mImpl->pool->statistics.declared_resources += mImpl->owner->resources.size();
    }
    mImpl->declared_encoder = native;
    return true;
}

bool ALMetalShadowMaterials::Pass::bind(
    ALMetalRenderEncoder& encoder, std::size_t request, std::string* error)
{
    if (!usesArgumentBuffer(request) || !encoder.nativeHandleForIndirect() ||
        (__bridge void*)mImpl->declared_encoder != encoder.nativeHandleForIndirect() ||
        encoder.nativeDeviceForIndirect() != (__bridge void*)mImpl->pool->device ||
        encoder.nativeCommandBufferForIndirect() != mImpl->command)
    {
        setError(error, "Metal shadow material binding requires its declared active encoder and table");
        return false;
    }
    auto* entry = mImpl->requests[request];
    if (!encoder.setFragmentBuffer(1, *entry->slot->buffer, 0, error)) return false;
    ++mImpl->pool->statistics.buffer_bind_calls;
    return true;
}

ALMetalShadowMaterialStats ALMetalShadowMaterials::stats() const noexcept
{
    const auto& pool = *mImpl->pool;
    auto result = pool.statistics;
    result.cached_tables = pool.cache.size();
    result.allocated_slots = pool.slots.size();
    result.allocated_buffer_bytes = pool.allocated_bytes;
    for (const auto& slot : pool.slots)
    {
        if (slot.use_count() == 1) continue;
        ++result.leased_slots;
        bool cached = false;
        for (const auto& [key, entry] : pool.cache)
            if (entry->slot.get() == slot.get())
            {
                cached = true;
                if (entry.use_count() > 1) ++result.command_referenced_tables;
                break;
            }
        if (!cached)
        {
            ++result.evicted_inflight_slots;
            ++result.command_referenced_tables;
        }
    }
    return result;
}
