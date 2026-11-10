/**
 * @file almetal_shadow_materials_test.mm
 * @brief Shadow material binding, cache and completion regressions.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalshadowmaterials.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <span>
#include <thread>
#include <vector>

namespace
{
constexpr unsigned width = 32, height = 8;
struct alignas(16) Params
{
    std::array<float, 16> mvp{};
    std::uint32_t selector = 0;
    float cutoff = 0.f;
    std::array<std::uint32_t, 2> padding{};
    Params()
    {
        mvp[0] = mvp[5] = mvp[10] = mvp[15] = 1.f;
    }
};
static_assert(sizeof(Params) == 80);

// Deliberately tiny actual resource ABI. World shader coverage is tested by
// the renderer's separate tests; this oracle observes table selection itself.
const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Params { float4x4 mvp; uint selector; float cutoff; uint2 padding; };
vertex float4 material_vertex(uint v [[vertex_id]], constant Params& p [[buffer(1)]])
{
    const float2 points[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
    return p.mvp*float4(points[v],.5,1);
}
float value(float a, constant Params& p) { return a >= p.cutoff ? a : 0.f; }
fragment float direct1(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]],
    constant Params& p [[buffer(0)]]) { return value(t.sample(s,float2(.5)).a,p); }
fragment float direct8(array<texture2d<float>,8> t [[texture(0)]], sampler s [[sampler(0)]],
    constant Params& p [[buffer(0)]]) { return value(t[min(p.selector,7u)].sample(s,float2(.5)).a,p); }
fragment float direct32(array<texture2d<float>,32> t [[texture(0)]], sampler s [[sampler(0)]],
    constant Params& p [[buffer(0)]]) { return value(t[min(p.selector,31u)].sample(s,float2(.5)).a,p); }
struct Material8 { array<texture2d<float>,8> textures [[id(0)]]; sampler s [[id(8)]]; };
struct Material32 { array<texture2d<float>,32> textures [[id(0)]]; sampler s [[id(32)]]; };
fragment float almetal_world_spot_shadow_coverage_eight_material_fragment(
    constant Params& p [[buffer(0)]], constant Material8& m [[buffer(1)]])
{ return value(m.textures[min(p.selector,7u)].sample(m.s,float2(.5)).a,p); }
fragment float almetal_world_spot_shadow_coverage_material_fragment(
    constant Params& p [[buffer(0)]], constant Material32& m [[buffer(1)]])
{ return value(m.textures[min(p.selector,31u)].sample(m.s,float2(.5)).a,p); }
)metal";

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 3> direct;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2> argument;
    std::unique_ptr<ALMetalSamplerState> sampler, other_sampler, incompatible_sampler;
    std::unique_ptr<ALMetalShadowMaterials> materials;

    explicit Fixture(ALMetalShadowMaterialLimits limits = {})
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::compileSource(*device, source, "Shadow material test", &error);
        REQUIRE(library);
        ALMetalRenderPipelineDescriptor p;
        p.library = library.get(); p.vertex_function = "material_vertex";
        p.colors[0].format = ALMetalPixelFormat::r32_float;
        p.label = "Shadow material test pipeline";
        const std::array<const char*, 3> direct_names{"direct1", "direct8", "direct32"};
        const std::array<const char*, 2> argument_names{
            "almetal_world_spot_shadow_coverage_eight_material_fragment",
            "almetal_world_spot_shadow_coverage_material_fragment"};
        for (unsigned i = 0; i < direct.size(); ++i)
        {
            p.fragment_function = direct_names[i];
            direct[i] = ALMetalRenderPipeline::create(*device, p, &error); REQUIRE(direct[i]);
        }
        for (unsigned i = 0; i < argument.size(); ++i)
        {
            p.fragment_function = argument_names[i];
            argument[i] = ALMetalRenderPipeline::create(*device, p, &error); REQUIRE(argument[i]);
        }
        ALMetalSamplerDescriptor s;
        incompatible_sampler = ALMetalSamplerState::create(*device, s, &error);
        REQUIRE(incompatible_sampler);
        s.support_argument_buffers = true;
        sampler = ALMetalSamplerState::create(*device, s, &error); REQUIRE(sampler);
        s.max_anisotropy = 4;
        other_sampler = ALMetalSamplerState::create(*device, s, &error); REQUIRE(other_sampler);
        materials = ALMetalShadowMaterials::create(*device, *library, &error, limits);
        INFO(error); REQUIRE(materials); REQUIRE(materials->supported());
    }

    std::unique_ptr<ALMetalTexture> texture(unsigned alpha, bool red_alpha = false)
    {
        ALMetalTextureDescriptor d;
        d.width = d.height = 1;
        if (red_alpha) d.swizzle[3] = ALMetalTextureSwizzle::red;
        d.label = "Shadow material sampled native view";
        auto t = ALMetalTexture::create(*device, d, &error); REQUIRE(t);
        upload(*t, alpha);
        return t;
    }
    void upload(ALMetalTexture& t, unsigned alpha)
    {
        const std::array<std::uint8_t, 4> bytes{37, 71, 113, std::uint8_t(alpha)};
        auto command = device->makeCommandBuffer("Shadow material upload", &error); REQUIRE(command);
        REQUIRE(t.upload(*command, 0, 0, {0, 0, 1, 1}, bytes.data(), 4, &error));
        REQUIRE(command->commitAndWait(&error));
    }
    std::unique_ptr<ALMetalRenderTarget> target()
    {
        ALMetalRenderTargetDescriptor d; d.width = width; d.height = height;
        d.colors[0].format = ALMetalPixelFormat::r32_float;
        d.label = "Shadow material readback target";
        auto t = ALMetalRenderTarget::create(*device, d, &error); REQUIRE(t); return t;
    }
    std::unique_ptr<ALMetalRenderEncoder> encoder(ALMetalCommandBuffer& c, ALMetalRenderTarget& t)
    {
        auto pass = t.makeRenderPass({}, &error); REQUIRE(pass);
        auto e = ALMetalRenderEncoder::begin(c, *pass, "Shadow material test pass", &error); REQUIRE(e);
        REQUIRE(e->setViewport({0, 0, width, height, 0, 1}, &error));
        REQUIRE(e->setCullMode(ALMetalCullMode::none, &error));
        return e;
    }
    void encode(ALMetalCommandBuffer& c, ALMetalRenderTarget& t,
                std::span<const ALMetalShadowMaterialRequest> requests,
                ALMetalShadowMaterials::Pass* material_pass = nullptr, float cutoff = 0.f)
    {
        REQUIRE(!requests.empty()); REQUIRE(width % requests.size() == 0);
        auto e = encoder(c, t);
        if (material_pass) REQUIRE(material_pass->declareResources(*e, &error));
        const unsigned band = width / unsigned(requests.size());
        for (unsigned i = 0; i < requests.size(); ++i)
        {
            const auto& r = requests[i];
            const unsigned slots = r.texture_count == 1 ? 1 : r.texture_count <= 8 ? 8 : 32;
            const bool use_argument = material_pass && material_pass->usesArgumentBuffer(i);
            REQUIRE(e->setPipeline(use_argument ? *argument[slots == 8 ? 0 : 1]
                                               : *direct[slots == 1 ? 0 : slots == 8 ? 1 : 2], &error));
            Params p; p.selector = slots - 1; p.cutoff = cutoff;
            p.mvp[14] = cutoff > 0.f ? .125f : 0.f; // Independent current-view data.
            REQUIRE(e->setVertexBytes(1, &p, sizeof(p), &error));
            REQUIRE(e->setFragmentBytes(0, &p, sizeof(p), &error));
            REQUIRE(e->setScissor({i * band, 0, band, height}, &error));
            if (use_argument) REQUIRE(material_pass->bind(*e, i, &error));
            else
            {
                REQUIRE(e->setFragmentSampler(0, r.sampler, &error));
                for (unsigned slot = 0; slot < slots; ++slot)
                    REQUIRE(e->setFragmentTexture(slot, r.textures[std::min(slot, r.texture_count - 1)], &error));
            }
            REQUIRE(e->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        }
        REQUIRE(e->end(&error));
    }
    std::array<float, width * height> pixels(ALMetalRenderTarget& t)
    {
        std::array<float, width * height> result{};
        REQUIRE(t.colorTexture(0)->readback(*device, 0, 0, {}, result.data(), width * sizeof(float), &error));
        return result;
    }
    auto pair(std::span<const ALMetalShadowMaterialRequest> requests, float cutoff = 0.f)
    {
        auto a = target(), b = target();
        auto c = device->makeCommandBuffer("Shadow material exact direct comparator", &error); REQUIRE(c);
        auto pass = materials->prepare(*c, requests, &error); INFO(error); REQUIRE(pass);
        encode(*c, *a, requests, nullptr, cutoff); encode(*c, *b, requests, pass.get(), cutoff);
        REQUIRE(c->commitAndWait(&error));
        const auto expected = pixels(*a), actual = pixels(*b); CHECK(actual == expected);
        return actual;
    }
    void waitForUncachedLeases()
    {
        REQUIRE(materials->stats().cached_tables == 0);
        for (unsigned i = 0; materials->stats().leased_slots != 0 && i < 1000; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        REQUIRE(materials->stats().leased_slots == 0);
    }
};

std::array<const ALMetalTexture*, 32> padded(const ALMetalTexture* first, const ALMetalTexture* last)
{
    std::array<const ALMetalTexture*, 32> textures; textures.fill(last); textures[0] = first; return textures;
}
}

TEST_CASE("almetal: shadow material keys normalize padding and reuse across view generations",
          "[almetal][shadow-materials]")
{
    @autoreleasepool
    {
        Fixture f; auto a = f.texture(64), b = f.texture(192), c = f.texture(224);
        auto eight = padded(a.get(), b.get());
        auto thirty_two = eight; std::fill(thirty_two.begin() + 8, thirty_two.end(), c.get());
        const std::array<ALMetalShadowMaterialRequest, 4> requests{{
            {eight.data(), 2, f.sampler.get()}, {eight.data(), 8, f.sampler.get()},
            {thirty_two.data(), 9, f.sampler.get()}, {thirty_two.data(), 32, f.sampler.get()}}};
        f.materials->beginFrame(10);
        const auto first = f.pair(requests);
        CHECK(std::abs(first[0] - 192.f / 255.f) < 1e-6f);
        CHECK(std::abs(first[24] - 224.f / 255.f) < 1e-6f);
        CHECK(f.materials->stats().table_creates == 2);
        CHECK(f.materials->stats().cache_hits == 2);
        const auto created = f.materials->stats().table_creates;
        f.materials->beginFrame(10); f.pair(requests, .95f); // New MVP/cutoff, same materials.
        f.materials->beginFrame(11); f.pair(requests);
        CHECK(f.materials->stats().table_creates == created);
        CHECK(f.materials->stats().cache_hits == 10);
        f.materials->beginFrame(12); CHECK(f.materials->stats().cached_tables == 2);
        f.materials->beginFrame(13); CHECK(f.materials->stats().cached_tables == 0);
        f.waitForUncachedLeases(); // Native completion wait need not finish owner callbacks.
        f.pair(requests);
        CHECK(f.materials->stats().allocated_slots == 2);
        CHECK(f.materials->stats().slot_reuses == 2);
        CHECK(f.materials->stats().table_creates == 4);
    }
}

TEST_CASE("almetal: shadow material keys observe sampler and current native view but not image contents",
          "[almetal][shadow-materials]")
{
    @autoreleasepool
    {
        Fixture f; auto a = f.texture(64), b = f.texture(192), replacement = f.texture(224), view = f.texture(224, true);
        auto textures = padded(a.get(), b.get());
        ALMetalShadowMaterialRequest request{textures.data(), 2, f.sampler.get()};
        f.materials->beginFrame(1); const auto original = f.pair(std::span(&request, 1));
        const auto creates = f.materials->stats().table_creates;
        f.upload(*b, 96); const auto changed_content = f.pair(std::span(&request, 1));
        CHECK(f.materials->stats().table_creates == creates);
        CHECK(changed_content != original); CHECK(std::abs(changed_content[0] - 96.f / 255.f) < 1e-6f);
        textures[1] = replacement.get(); const auto changed_native = f.pair(std::span(&request, 1));
        CHECK(f.materials->stats().table_creates == creates + 1); CHECK(changed_native != changed_content);
        textures[1] = view.get(); const auto swizzled = f.pair(std::span(&request, 1));
        CHECK(f.materials->stats().table_creates == creates + 2); CHECK(swizzled != changed_native);
        CHECK(std::abs(swizzled[0] - 37.f / 255.f) < 1e-6f);
        request.sampler = f.other_sampler.get(); f.pair(std::span(&request, 1));
        CHECK(f.materials->stats().table_creates == creates + 3);
        request.texture_count = 1;
        auto command = f.device->makeCommandBuffer("Single material direct policy", &f.error); REQUIRE(command);
        auto pass = f.materials->prepare(*command, std::span(&request, 1), &f.error); REQUIRE(pass);
        CHECK_FALSE(pass->usesArgumentBuffer(0)); CHECK(pass->requestCount() == 1);
    }
}

TEST_CASE("almetal: shadow material capacity counts active and evicted pending leases and falls back exactly",
          "[almetal][shadow-materials][completion]")
{
    @autoreleasepool
    {
        Fixture f({1, 1024 * 1024}); auto a = f.texture(64), b = f.texture(128), c = f.texture(224);
        auto old_textures = padded(a.get(), b.get()), new_textures = padded(a.get(), c.get());
        ALMetalShadowMaterialRequest old_request{old_textures.data(), 2, f.sampler.get()}, new_request{new_textures.data(), 2, f.sampler.get()};
        auto old_target = f.target(), fallback_target = f.target(), direct_target = f.target();
        auto command = f.device->makeCommandBuffer("Two material views one pending parent", &f.error); REQUIRE(command);
        f.materials->beginFrame(1);
        auto old_pass = f.materials->prepare(*command, std::span(&old_request, 1), &f.error); REQUIRE(old_pass);
        REQUIRE(old_pass->usesArgumentBuffer(0)); f.encode(*command, *old_target, std::span(&old_request, 1), old_pass.get());
        old_pass.reset(); f.materials->beginFrame(2); f.materials->beginFrame(3);
        CHECK(f.materials->stats().evicted_inflight_slots == 1);
        auto new_pass = f.materials->prepare(*command, std::span(&new_request, 1), &f.error); REQUIRE(new_pass);
        CHECK_FALSE(new_pass->usesArgumentBuffer(0));
        CHECK(f.materials->stats().allocated_slots == 1); CHECK(f.materials->stats().capacity_fallbacks == 1);
        f.encode(*command, *fallback_target, std::span(&new_request, 1), new_pass.get());
        f.encode(*command, *direct_target, std::span(&new_request, 1));
        REQUIRE(command->commitAndWait(&f.error)); new_pass.reset(); command.reset();
        CHECK(f.pixels(*fallback_target) == f.pixels(*direct_target));
        CHECK(std::abs(f.pixels(*old_target)[0] - 128.f / 255.f) < 1e-6f);
        f.waitForUncachedLeases();
        const auto reused = f.pair(std::span(&new_request, 1));
        CHECK(std::abs(reused[0] - 224.f / 255.f) < 1e-6f);
        CHECK(f.materials->stats().allocated_slots == 1); CHECK(f.materials->stats().slot_reuses == 1);
        Fixture byte_limited({8, 1}); auto low_a = byte_limited.texture(64), low_b = byte_limited.texture(192);
        auto low_textures = padded(low_a.get(), low_b.get());
        ALMetalShadowMaterialRequest low{low_textures.data(), 2, byte_limited.sampler.get()};
        byte_limited.pair(std::span(&low, 1));
        CHECK(byte_limited.materials->stats().allocated_slots == 0);
        CHECK(byte_limited.materials->stats().capacity_fallbacks == 1);

        // A genuinely committed GPU lease also remains charged after lookup
        // eviction; the later command must take the exact direct path.
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)f.device->nativeHandle();
        id<MTLSharedEvent> gate = [native_device newSharedEvent]; REQUIRE(gate);
        struct Signal { id<MTLSharedEvent> event; ~Signal() { event.signaledValue = 1; } } signal{gate};
        auto held_command = f.device->makeCommandBuffer("Committed material slot lease", &f.error); REQUIRE(held_command);
        auto held_native = (__bridge id<MTLCommandBuffer>)held_command->nativeHandle();
        [held_native encodeWaitForEvent:gate value:1];
        auto held_pass = f.materials->prepare(*held_command, std::span(&new_request, 1), &f.error); REQUIRE(held_pass);
        f.encode(*held_command, *old_target, std::span(&new_request, 1), held_pass.get());
        REQUIRE(held_command->commit(&f.error)); held_pass.reset(); held_command.reset(); held_native = nil;
        f.materials->beginFrame(4); f.materials->beginFrame(5);
        CHECK(f.materials->stats().evicted_inflight_slots == 1);
        auto later_command = f.device->makeCommandBuffer("Capacity fallback behind pending GPU", &f.error); REQUIRE(later_command);
        auto later_pass = f.materials->prepare(*later_command, std::span(&old_request, 1), &f.error); REQUIRE(later_pass);
        CHECK_FALSE(later_pass->usesArgumentBuffer(0));
        CHECK(f.materials->stats().allocated_slots == 1);
        f.encode(*later_command, *fallback_target, std::span(&old_request, 1), later_pass.get());
        REQUIRE(later_command->commit(&f.error)); gate.signaledValue = 1;
        REQUIRE(later_command->waitUntilCompleted(&f.error));
        CHECK(std::abs(f.pixels(*fallback_target)[0] - 128.f / 255.f) < 1e-6f);
    }
}

TEST_CASE("almetal: shadow material invalid requests and wrong encoder cannot publish bindings",
          "[almetal][shadow-materials]")
{
    @autoreleasepool
    {
        Fixture f; auto a = f.texture(64), b = f.texture(192); auto textures = padded(a.get(), b.get());
        ALMetalShadowMaterialRequest valid{textures.data(), 2, f.sampler.get()};
        ALMetalTextureDescriptor array_desc; array_desc.type = ALMetalTextureType::texture_2d_array;
        array_desc.array_length = 2; array_desc.label = "Invalid sampled array material";
        auto array_texture = ALMetalTexture::create(*f.device, array_desc, &f.error); REQUIRE(array_texture);
        ALMetalTextureDescriptor unreadable_desc; unreadable_desc.usage = ALMetalTextureUsage::render_target;
        unreadable_desc.label = "Invalid render-only material";
        auto unreadable = ALMetalTexture::create(*f.device, unreadable_desc, &f.error); REQUIRE(unreadable);
        auto command = f.device->makeCommandBuffer("Material preflight rejection", &f.error); REQUIRE(command);
        const auto initial = f.materials->stats();
        const auto committed_before = ALMetalDevice::committedCommandBufferCount();
        for (unsigned bad : {0u, 1u, 2u, 3u, 4u, 5u, 6u})
        {
            auto invalid = valid;
            auto invalid_textures = textures;
            if (bad == 0) invalid.texture_count = 0;
            if (bad == 1) invalid.texture_count = 33;
            if (bad == 2) invalid.textures = nullptr;
            if (bad == 3) invalid.sampler = f.incompatible_sampler.get();
            if (bad == 4) { invalid_textures[1] = nullptr; invalid.textures = invalid_textures.data(); }
            if (bad == 5) { invalid_textures[1] = array_texture.get(); invalid.textures = invalid_textures.data(); }
            if (bad == 6) { invalid_textures[1] = unreadable.get(); invalid.textures = invalid_textures.data(); }
            const std::array<ALMetalShadowMaterialRequest, 2> late{valid, invalid};
            CHECK_FALSE(f.materials->prepare(*command, late, &f.error));
            CHECK(f.materials->stats().prepared_passes == initial.prepared_passes);
            CHECK(f.materials->stats().table_creates == initial.table_creates);
            CHECK(f.materials->stats().allocated_slots == initial.allocated_slots);
            CHECK(f.materials->stats().cache_hits == initial.cache_hits);
            CHECK(f.materials->stats().cache_misses == initial.cache_misses);
            CHECK(ALMetalDevice::committedCommandBufferCount() == committed_before);
        }
        auto pass = f.materials->prepare(*command, std::span(&valid, 1), &f.error); REQUIRE(pass);
        auto other_command = f.device->makeCommandBuffer("Wrong material parent", &f.error); REQUIRE(other_command);
        auto target = f.target(); auto other_encoder = f.encoder(*other_command, *target);
        CHECK_FALSE(pass->declareResources(*other_encoder, &f.error));
        CHECK_FALSE(pass->bind(*other_encoder, 0, &f.error)); REQUIRE(other_encoder->end(&f.error));
        auto own_encoder = f.encoder(*command, *target);
        REQUIRE(own_encoder->setPipeline(*f.argument[0], &f.error));
        CHECK_FALSE(pass->bind(*own_encoder, 0, &f.error)); // Declaration is mandatory.
        REQUIRE(pass->declareResources(*own_encoder, &f.error));
        REQUIRE(pass->declareResources(*own_encoder, &f.error)); // Idempotent; no duplicate inventory.
        CHECK(f.materials->stats().declared_resources == 2);
        REQUIRE(pass->bind(*own_encoder, 0, &f.error));
        CHECK_FALSE(pass->bind(*own_encoder, 1, &f.error));
        REQUIRE(own_encoder->end(&f.error));
        CHECK_FALSE(pass->bind(*own_encoder, 0, &f.error));
        CHECK_FALSE(pass->declareResources(*own_encoder, &f.error));
        REQUIRE(command->commitAndWait(&f.error));
        CHECK_FALSE(f.materials->prepare(*command, std::span(&valid, 1), &f.error));
    }
}

TEST_CASE("almetal: shadow material indirect resources outlive helper and retire without completion cycles",
          "[almetal][shadow-materials][completion]")
{
    @autoreleasepool
    {
        Fixture f; auto target = f.target();
        __weak id<MTLTexture> weak_texture = nil;
        std::uint64_t buffers_destroyed_before = 0;
        id<MTLDevice> device = (__bridge id<MTLDevice>)f.device->nativeHandle();
        id<MTLSharedEvent> gate = [device newSharedEvent]; REQUIRE(gate);
        struct Signal { id<MTLSharedEvent> event; ~Signal() { event.signaledValue = 1; } } signal{gate};
        SECTION("committed pending GPU command retains the table and sampled textures")
        {
            @autoreleasepool
            {
                auto a = f.texture(64), b = f.texture(192); weak_texture = (__bridge id<MTLTexture>)b->nativeHandle();
                auto textures = padded(a.get(), b.get()); ALMetalShadowMaterialRequest r{textures.data(), 2, f.sampler.get()};
                buffers_destroyed_before = alMetalResourceAllocationStats().buffers_destroyed;
                auto command = f.device->makeCommandBuffer("Destroyed material owner while GPU gated", &f.error); REQUIRE(command);
                auto native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
                [native encodeWaitForEvent:gate value:1];
                auto pass = f.materials->prepare(*command, std::span(&r, 1), &f.error); REQUIRE(pass);
                f.encode(*command, *target, std::span(&r, 1), pass.get());
                pass.reset(); f.materials.reset(); a.reset(); b.reset();
                CHECK(alMetalResourceAllocationStats().buffers_destroyed == buffers_destroyed_before);
                REQUIRE(command->commit(&f.error)); command.reset(); native = nil;
            }
            CHECK(weak_texture != nil); // GPU has not consumed this indirect reference yet.
            gate.signaledValue = 1; REQUIRE(f.device->waitUntilIdle(&f.error));
            CHECK(std::abs(f.pixels(*target)[0] - 192.f / 255.f) < 1e-6f);
        }
        SECTION("abandonment releases uncommitted owners without callbacks")
        {
            @autoreleasepool
            {
                auto a = f.texture(64), b = f.texture(192); weak_texture = (__bridge id<MTLTexture>)b->nativeHandle();
                auto textures = padded(a.get(), b.get()); ALMetalShadowMaterialRequest r{textures.data(), 2, f.sampler.get()};
                buffers_destroyed_before = alMetalResourceAllocationStats().buffers_destroyed;
                auto command = f.device->makeCommandBuffer("Abandoned material owner", &f.error); REQUIRE(command);
                auto pass = f.materials->prepare(*command, std::span(&r, 1), &f.error); REQUIRE(pass);
                f.encode(*command, *target, std::span(&r, 1), pass.get());
                pass.reset(); f.materials.reset(); a.reset(); b.reset();
                CHECK(alMetalResourceAllocationStats().buffers_destroyed == buffers_destroyed_before);
                CHECK(weak_texture != nil); command.reset();
            }
        }
        // Completion callbacks can retire on a driver thread just after the
        // native wait. Poll only CPU ownership, with a fixed short test bound.
        for (unsigned i = 0; (weak_texture != nil ||
             alMetalResourceAllocationStats().buffers_destroyed == buffers_destroyed_before) && i < 1000; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        CHECK(weak_texture == nil);
        CHECK(alMetalResourceAllocationStats().buffers_destroyed == buffers_destroyed_before + 1);
    }
}

TEST_CASE("almetal: shadow material native metadata and slot scans remain bounded at saturation",
          "[almetal][shadow-materials][native-work]")
{
    @autoreleasepool
    {
        // Several admitted tables in one family must share one native
        // metadata encoder; saturation alone would not prove miss-path reuse.
        {
            Fixture admitted({8, 1024 * 1024});
            std::array<std::unique_ptr<ALMetalTexture>, 5> owned;
            for (unsigned i = 0; i < owned.size(); ++i) owned[i] = admitted.texture(32 + 32 * i);
            std::array<std::array<const ALMetalTexture*, 32>, 4> views;
            std::array<ALMetalShadowMaterialRequest, 4> requests;
            admitted.materials->beginFrame(1);
            for (unsigned count : {2u, 9u})
            {
                for (unsigned i = 0; i < requests.size(); ++i)
                {
                    views[i] = padded(owned[0].get(), owned[i + 1].get());
                    requests[i] = {views[i].data(), count, admitted.sampler.get()};
                }
                const auto before = admitted.materials->stats();
                const auto pixels = admitted.pair(requests);
                const auto after = admitted.materials->stats();
                CHECK(after.table_creates - before.table_creates == 4);
                CHECK(after.argument_encoder_creations - before.argument_encoder_creations == 1);
                CHECK(after.capacity_fallbacks == before.capacity_fallbacks);
                for (unsigned i = 0; i < requests.size(); ++i)
                    CHECK(std::abs(pixels[i * 8] - float(64 + 32 * i) / 255.f) < 1e-6f);
            }
        }
        Fixture f({2, 1024 * 1024});
        constexpr std::array<unsigned, 8> alpha{32, 64, 96, 128, 160, 192, 224, 255};
        std::array<std::unique_ptr<ALMetalTexture>, 8> textures;
        for (unsigned i = 0; i < textures.size(); ++i) textures[i] = f.texture(alpha[i]);
        const std::array<std::array<const ALMetalTexture*, 32>, 2> initial_views{
            padded(textures[0].get(), textures[1].get()),
            padded(textures[0].get(), textures[2].get())};
        const std::array<ALMetalShadowMaterialRequest, 2> initial{{
            {initial_views[0].data(), 2, f.sampler.get()},
            {initial_views[1].data(), 9, f.sampler.get()}}};
        f.materials->beginFrame(1);
        const auto before_initial = f.materials->stats();
        const auto populated = f.pair(initial);
        CHECK(std::abs(populated[0] - 64.f / 255.f) < 1e-6f);
        CHECK(std::abs(populated[16] - 96.f / 255.f) < 1e-6f);
        const auto after_initial = f.materials->stats();
        // Factory reflection is deliberately excluded. Each family prepares
        // its native encoder once, while every table keeps independent bytes.
        CHECK(after_initial.argument_encoder_creations - before_initial.argument_encoder_creations == 2);
        CHECK(after_initial.table_creates - before_initial.table_creates == 2);

        std::array<std::array<const ALMetalTexture*, 32>, 32> miss_views;
        std::array<ALMetalShadowMaterialRequest, 32> misses;
        for (unsigned i = 0; i < misses.size(); ++i)
        {
            miss_views[i] = padded(textures[i / 8].get(), textures[i % 8].get());
            // Neither pair duplicates the original entry in its ABI family:
            // original(0,1) is eight-slot; here it is32, and(0,2) is vice versa.
            misses[i] = {miss_views[i].data(), (i & 1u) ? 9u : 2u, f.sampler.get()};
        }
        // Repeat one new key within its original ABI family. This disables
        // one-use pressure bypass while retaining32draws and31unique misses.
        misses.back() = misses[1];
        const auto before_full = f.materials->stats();
        const auto direct_fallback = f.pair(misses);
        const auto after_full = f.materials->stats();
        for (unsigned i = 0; i < misses.size(); ++i)
            CHECK(std::abs(direct_fallback[i] - float(alpha[i + 1 == misses.size() ? 1 : i % 8]) / 255.f) < 1e-6f);
        CHECK(after_full.argument_encoder_creations == before_full.argument_encoder_creations);
        CHECK(after_full.slot_scan_visits == before_full.slot_scan_visits);
        CHECK(after_full.capacity_fallbacks - before_full.capacity_fallbacks == misses.size() - 1);
        CHECK(after_full.direct_requests - before_full.direct_requests == misses.size());
        CHECK(after_full.table_creates == before_full.table_creates);
        CHECK(after_full.allocated_slots == 2);

        auto held_target = f.target(), expected_target = f.target(), actual_target = f.target();
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)f.device->nativeHandle();
        id<MTLSharedEvent> gate = [native_device newSharedEvent]; REQUIRE(gate);
        struct Signal { id<MTLSharedEvent> event; ~Signal() { event.signaledValue = 1; } } signal{gate};
        auto held_command = f.device->makeCommandBuffer("Both material ABI slots pending on GPU", &f.error);
        REQUIRE(held_command);
        auto held_native = (__bridge id<MTLCommandBuffer>)held_command->nativeHandle();
        [held_native encodeWaitForEvent:gate value:1];
        auto held_pass = f.materials->prepare(*held_command, initial, &f.error); REQUIRE(held_pass);
        f.encode(*held_command, *held_target, initial, held_pass.get());
        REQUIRE(held_command->commit(&f.error)); held_pass.reset(); held_command.reset(); held_native = nil;
        f.materials->beginFrame(2); f.materials->beginFrame(3);
        CHECK(f.materials->stats().cached_tables == 0);
        CHECK(f.materials->stats().evicted_inflight_slots == 2);

        const auto before_pending = f.materials->stats();
        auto later_command = f.device->makeCommandBuffer("Saturated requests behind pending material tables", &f.error);
        REQUIRE(later_command);
        auto later_pass = f.materials->prepare(*later_command, misses, &f.error); REQUIRE(later_pass);
        for (unsigned i = 0; i < misses.size(); ++i) CHECK_FALSE(later_pass->usesArgumentBuffer(i));
        f.encode(*later_command, *expected_target, misses);
        f.encode(*later_command, *actual_target, misses, later_pass.get());
        const auto after_pending = f.materials->stats();
        CHECK(after_pending.argument_encoder_creations == before_pending.argument_encoder_creations);
        // At most both loops inspect two slots for the first failure in each
        // family. Remaining requests cannot repeat native metadata/scans.
        CHECK(after_pending.slot_scan_visits - before_pending.slot_scan_visits <= 2 * 2 * 2);
        CHECK(after_pending.capacity_fallbacks - before_pending.capacity_fallbacks == misses.size() - 1);
        CHECK(after_pending.direct_requests - before_pending.direct_requests == misses.size());
        CHECK(after_pending.table_creates == before_pending.table_creates);
        CHECK(after_pending.allocated_slots == 2);
        REQUIRE(later_command->commit(&f.error)); gate.signaledValue = 1;
        REQUIRE(later_command->waitUntilCompleted(&f.error));
        const auto actual = f.pixels(*actual_target), expected = f.pixels(*expected_target);
        CHECK(actual == expected);
        for (unsigned i = 0; i < misses.size(); ++i)
            CHECK(std::abs(actual[i] - float(alpha[i + 1 == misses.size() ? 1 : i % 8]) / 255.f) < 1e-6f);
        const auto held = f.pixels(*held_target);
        CHECK(std::abs(held[0] - 64.f / 255.f) < 1e-6f);
        CHECK(std::abs(held[16] - 96.f / 255.f) < 1e-6f);
    }
}

TEST_CASE("almetal: shadow material pressure bypass keeps cached hits and validates every new request",
          "[almetal][shadow-materials][pressure]")
{
    @autoreleasepool
    {
        Fixture f({2, 1024 * 1024});
        std::array<std::unique_ptr<ALMetalTexture>, 7> textures;
        for (unsigned i = 0; i < textures.size(); ++i) textures[i] = f.texture(32 + i * 32);
        const std::array<std::array<const ALMetalTexture*, 32>, 2> cached_views{
            padded(textures[0].get(), textures[1].get()),
            padded(textures[0].get(), textures[2].get())};
        const std::array<ALMetalShadowMaterialRequest, 2> cached{{
            {cached_views[0].data(), 2, f.sampler.get()},
            {cached_views[1].data(), 9, f.sampler.get()}}};
        std::array<std::array<const ALMetalTexture*, 32>, 4> new_views;
        std::array<ALMetalShadowMaterialRequest, 4> new_once;
        for (unsigned i = 0; i < new_once.size(); ++i)
        {
            new_views[i] = padded(textures[0].get(), textures[i + 3].get());
            new_once[i] = {new_views[i].data(), (i & 1u) ? 9u : 2u, f.sampler.get()};
        }
        f.materials->beginFrame(1);
        SECTION("an entirely new one-use working set exceeding storage stays direct")
        {
            const auto before = f.materials->stats();
            const auto output = f.pair(new_once);
            const auto after = f.materials->stats();
            for (unsigned i = 0; i < new_once.size(); ++i)
                CHECK(std::abs(output[i * 8] - float(128 + i * 32) / 255.f) < 1e-6f);
            CHECK(after.table_creates == before.table_creates);
            CHECK(after.allocated_slots == 0); CHECK(after.allocated_buffer_bytes == 0);
            CHECK(after.argument_encoder_creations == before.argument_encoder_creations);
            CHECK(after.slot_scan_visits == before.slot_scan_visits);
            CHECK(after.capacity_fallbacks - before.capacity_fallbacks == 4);
            CHECK(after.direct_requests - before.direct_requests == 4);
            CHECK(after.buffer_bind_calls == before.buffer_bind_calls);
            CHECK(after.declared_resources == before.declared_resources);
        }
        SECTION("existing cached requests remain argument draws while new pressure requests stay direct")
        {
            f.pair(cached); // Equal-to-capacity admission still creates both ABI families.
            const auto single_view = padded(textures[6].get(), textures[6].get());
            const std::array<ALMetalShadowMaterialRequest, 8> requests{{
                cached[0], cached[1], new_once[0], new_once[1], new_once[2], new_once[3],
                {cached_views[0].data(), 1, f.sampler.get()},
                {single_view.data(), 1, f.sampler.get()}}};
            const auto before = f.materials->stats();
            auto expected_target = f.target(), actual_target = f.target();
            auto command = f.device->makeCommandBuffer("Cached hits mixed with material pressure", &f.error);
            REQUIRE(command);
            auto pass = f.materials->prepare(*command, requests, &f.error); REQUIRE(pass);
            for (unsigned i = 0; i < requests.size(); ++i)
                CHECK(pass->usesArgumentBuffer(i) == (i < 2));
            f.encode(*command, *expected_target, requests);
            f.encode(*command, *actual_target, requests, pass.get());
            REQUIRE(command->commitAndWait(&f.error));
            const auto actual = f.pixels(*actual_target), expected = f.pixels(*expected_target);
            CHECK(actual == expected);
            constexpr std::array<unsigned, 8> alpha{64, 96, 128, 160, 192, 224, 32, 224};
            for (unsigned i = 0; i < alpha.size(); ++i)
                CHECK(std::abs(actual[i * 4] - float(alpha[i]) / 255.f) < 1e-6f);
            const auto after = f.materials->stats();
            CHECK(after.cache_hits - before.cache_hits == 2);
            CHECK(after.cache_misses - before.cache_misses == 4);
            CHECK(after.capacity_fallbacks - before.capacity_fallbacks == 4);
            CHECK(after.direct_requests - before.direct_requests == 6); // Four pressure + two single-texture.
            CHECK(after.buffer_bind_calls - before.buffer_bind_calls == 2);
            CHECK(after.declared_resources - before.declared_resources == 3);
            CHECK(after.table_creates == before.table_creates);
            CHECK(after.allocated_slots == 2);
            CHECK(after.argument_encoder_creations == before.argument_encoder_creations);
            CHECK(after.slot_scan_visits == before.slot_scan_visits);
        }
        SECTION("pressure never skips a later invalid request")
        {
            auto invalid = new_once;
            invalid.back().texture_count = 33;
            const auto before = f.materials->stats();
            const auto commits = ALMetalDevice::committedCommandBufferCount();
            auto command = f.device->makeCommandBuffer("Pressure late-invalid input", &f.error); REQUIRE(command);
            CHECK_FALSE(f.materials->prepare(*command, invalid, &f.error));
            const auto after = f.materials->stats();
            CHECK(after.prepared_passes == before.prepared_passes);
            CHECK(after.table_creates == before.table_creates);
            CHECK(after.cache_misses == before.cache_misses);
            CHECK(after.capacity_fallbacks == before.capacity_fallbacks);
            CHECK(after.argument_encoder_creations == before.argument_encoder_creations);
            CHECK(after.allocated_slots == before.allocated_slots);
            CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
        }
    }
}
