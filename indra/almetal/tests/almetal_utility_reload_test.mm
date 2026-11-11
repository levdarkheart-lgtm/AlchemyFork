/**
 * @file almetal_utility_reload_test.mm
 * @brief Real GPU utility reload, retry and resource-survival regressions.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaltextureops.h"
#include "../almetalterrainpaint.h"
#include "../almetalretainedbackground.h"

#import <Metal/Metal.h>
#import <objc/runtime.h>

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
// MTLDevice.h declares this synchronous factory with a retained ("new")
// result and NSError** out parameter. Preserve that ARC convention when
// forwarding its IMP. Only this serial test's device receives one injected
// failure; other devices and every other call use the real Metal factory.
id<MTLRenderPipelineState> pipelineFactory(
    id device, SEL selector, MTLRenderPipelineDescriptor* descriptor,
    NSError* __autoreleasing* error) __attribute__((ns_returns_retained));

struct PipelineFailure
{
    static PipelineFailure* active;
    Method method = nullptr;
    IMP original = nullptr;
    id device = nil;
    unsigned fail_at = 1, calls = 0;

    PipelineFailure(const ALMetalDevice& owner, unsigned attempt = 1)
    : device((__bridge id)owner.nativeHandle()), fail_at(attempt)
    {
        REQUIRE(active == nullptr);
        const SEL selector = @selector(newRenderPipelineStateWithDescriptor:error:);
        REQUIRE([device respondsToSelector:selector]);
        method = class_getInstanceMethod(object_getClass(device), selector);
        REQUIRE(method != nullptr);
        original = method_getImplementation(method);
        REQUIRE(original != nullptr);
        active = this;
        method_setImplementation(method, reinterpret_cast<IMP>(&pipelineFactory));
    }
    ~PipelineFailure()
    {
        method_setImplementation(method, original);
        active = nullptr;
    }
    PipelineFailure(const PipelineFailure&) = delete;
    PipelineFailure& operator=(const PipelineFailure&) = delete;
};
PipelineFailure* PipelineFailure::active = nullptr;

id<MTLRenderPipelineState> pipelineFactory(
    id device, SEL selector, MTLRenderPipelineDescriptor* descriptor,
    NSError* __autoreleasing* error)
{
    auto& state = *PipelineFailure::active;
    if (device == state.device && ++state.calls == state.fail_at)
    {
        if (error) *error = [NSError errorWithDomain:@"AlchemyUtilityReloadInjectedFailure"
            code:1 userInfo:@{NSLocalizedDescriptionKey:@"Injected utility reload PSO failure"}];
        return nil;
    }
    return reinterpret_cast<decltype(&pipelineFactory)>(state.original)(
        device, selector, descriptor, error);
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    Fixture()
    {
        device = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
    }
    std::unique_ptr<ALMetalTexture> texture(ALMetalPixelFormat format,
        unsigned width, unsigned height, bool destination = true, unsigned levels = 1)
    {
        ALMetalTextureDescriptor d;
        d.format = format; d.width = width; d.height = height;
        d.mipmap_level_count = levels;
        d.storage = ALMetalStorageMode::private_gpu;
        d.usage = ALMetalTextureUsage::shader_read;
        if (destination) d.usage = d.usage | ALMetalTextureUsage::render_target;
        d.label = "Utility reload retained texture";
        auto result = ALMetalTexture::create(*device, d, &error);
        INFO(error); REQUIRE(result);
        return result;
    }
    std::unique_ptr<ALMetalCommandBuffer> command()
    {
        auto result = device->makeCommandBuffer("Utility shader reload regression", &error);
        INFO(error); REQUIRE(result); return result;
    }
    std::vector<std::uint8_t> read(const ALMetalTexture& texture, unsigned components = 4,
                                 unsigned level = 0)
    {
        const auto width = std::max(1u, texture.descriptor().width >> level);
        const auto height = std::max(1u, texture.descriptor().height >> level);
        std::vector<std::uint8_t> result(width * height * components);
        REQUIRE(texture.readback(*device, level, 0, {}, result.data(), width * components, &error));
        return result;
    }
    void checkInjected() { CHECK(error.find("Injected utility reload PSO failure") != std::string::npos); }
};
}

TEST_CASE("almetal: resampler reload stages every used variant and retains encoded work",
          "[almetal][utility-reload]")
{
    @autoreleasepool
    {
        Fixture f;
        auto owner = ALMetalTextureResampler::create(*f.device, &f.error); REQUIRE(owner);
        auto source = f.texture(ALMetalPixelFormat::r8_unorm, 4, 4, false);
        auto rgba = f.texture(ALMetalPixelFormat::rgba8_unorm, 4, 4, false);
        auto old_small = f.texture(ALMetalPixelFormat::r8_unorm, 2, 2);
        auto old_flip = f.texture(ALMetalPixelFormat::r8_unorm, 4, 4);
        auto old_rgba = f.texture(ALMetalPixelFormat::rgba8_unorm, 2, 2);
        auto small = f.texture(ALMetalPixelFormat::r8_unorm, 2, 2);
        auto flip = f.texture(ALMetalPixelFormat::r8_unorm, 4, 4);
        auto color = f.texture(ALMetalPixelFormat::rgba8_unorm, 2, 2);
        const std::array<std::uint8_t, 16> pixels{
            16,16,64,64,16,16,64,64,128,128,240,240,128,128,240,240};
        std::array<std::uint8_t, 64> colors{};
        for (unsigned i = 0; i < 16; ++i)
            for (unsigned c = 0; c < 4; ++c) colors[i * 4 + c] = pixels[i];
        const auto source_handle = source->nativeHandle();
        const auto generation = owner->shaderReloadGeneration();
        auto old_command = f.command();
        REQUIRE(source->upload(*old_command, 0, 0, {}, pixels.data(), 4, &f.error));
        REQUIRE(rgba->upload(*old_command, 0, 0, {}, colors.data(), 16, &f.error));
        REQUIRE(owner->encode(*old_command, *source, *old_small, &f.error));
        REQUIRE(owner->encode(*old_command, *source, *old_flip, true, &f.error));
        REQUIRE(owner->encode(*old_command, *rgba, *old_rgba, &f.error));
        ALMetalShaderLibrary::requestBuiltinReload();
        CHECK(ALMetalShaderLibrary::builtinReloadGeneration() == generation + 1);
        {
            PipelineFailure failure(*f.device, 3);
            auto rejected = f.command();
            CHECK_FALSE(owner->encode(*rejected, *source, *small, &f.error));
            CHECK(failure.calls == 3); f.checkInjected();
            CHECK(owner->shaderReloadGeneration() == generation);
        }
        auto command = f.command();
        REQUIRE(owner->encode(*command, *source, *small, &f.error));
        REQUIRE(owner->encode(*command, *source, *flip, true, &f.error));
        REQUIRE(owner->encode(*command, *rgba, *color, &f.error));
        CHECK(owner->shaderReloadGeneration() == generation + 1);
        CHECK(source->nativeHandle() == source_handle);
        // The old PSO wrappers were replaced before this command was committed.
        REQUIRE(old_command->commit(&f.error));
        REQUIRE(command->commitAndWait(&f.error));
        const std::vector<std::uint8_t> expected{16,64,128,240};
        CHECK(f.read(*old_small, 1) == expected); CHECK(f.read(*small, 1) == expected);
        std::vector<std::uint8_t> flipped;
        for (int row = 3; row >= 0; --row)
            flipped.insert(flipped.end(), pixels.begin() + row * 4, pixels.begin() + row * 4 + 4);
        CHECK(f.read(*old_flip, 1) == flipped); CHECK(f.read(*flip, 1) == flipped);
        std::vector<std::uint8_t> expected_color;
        for (auto value : expected) expected_color.insert(expected_color.end(), 4, value);
        CHECK(f.read(*old_rgba) == expected_color); CHECK(f.read(*color) == expected_color);
        CHECK(f.read(*source, 1) == std::vector<std::uint8_t>(pixels.begin(), pixels.end()));
    }
}

TEST_CASE("almetal: normal generator reload retries and preserves generated mip chains",
          "[almetal][utility-reload]")
{
    @autoreleasepool
    {
        Fixture f;
        auto owner = ALMetalNormalMapGenerator::create(*f.device, &f.error); REQUIRE(owner);
        auto source = f.texture(ALMetalPixelFormat::rgba8_unorm, 4, 4, false);
        auto old_normal = f.texture(ALMetalPixelFormat::rgba8_unorm, 4, 4, true, 3);
        auto normal = f.texture(ALMetalPixelFormat::rgba8_unorm, 4, 4, true, 3);
        std::array<std::uint8_t, 64> pixels{};
        for (unsigned i = 0; i < 16; ++i)
        { pixels[i*4] = pixels[i*4+1] = pixels[i*4+2] = 64; pixels[i*4+3] = 255; }
        auto old_command = f.command();
        REQUIRE(source->upload(*old_command, 0, 0, {}, pixels.data(), 16, &f.error));
        REQUIRE(owner->encode(*old_command, *source, *old_normal, 2.f, false, &f.error));
        REQUIRE(old_normal->generateMipmaps(*old_command, &f.error));
        const auto generation = owner->shaderReloadGeneration();
        ALMetalShaderLibrary::requestBuiltinReload();
        {
            PipelineFailure failure(*f.device);
            auto rejected = f.command();
            CHECK_FALSE(owner->encode(*rejected, *source, *normal, 2.f, true, &f.error));
            CHECK(failure.calls == 1); f.checkInjected();
            CHECK(owner->shaderReloadGeneration() == generation);
        }
        auto command = f.command();
        REQUIRE(owner->encode(*command, *source, *normal, 2.f, true, &f.error));
        REQUIRE(normal->generateMipmaps(*command, &f.error));
        CHECK(owner->shaderReloadGeneration() == generation + 1);
        REQUIRE(old_command->commit(&f.error)); REQUIRE(command->commitAndWait(&f.error));
        for (unsigned level = 0; level < 3; ++level)
        {
            const auto old_pixels = f.read(*old_normal, 4, level);
            const auto new_pixels = f.read(*normal, 4, level);
            for (std::size_t i = 0; i < old_pixels.size(); i += 4)
            {
                CHECK(std::abs(int(old_pixels[i]) - 128) <= 1);
                CHECK(std::abs(int(old_pixels[i+1]) - 128) <= 1);
                CHECK(old_pixels[i+2] == 255); CHECK(std::abs(int(old_pixels[i+3]) - 64) <= 1);
                CHECK(std::abs(int(new_pixels[i]) - 128) <= 1);
                CHECK(std::abs(int(new_pixels[i+1]) - 128) <= 1);
                CHECK(new_pixels[i+2] == 255); CHECK(std::abs(int(new_pixels[i+3]) - 191) <= 1);
            }
        }
        CHECK(f.read(*source) == std::vector<std::uint8_t>(pixels.begin(), pixels.end()));
    }
}

TEST_CASE("almetal: terrain reload preserves external inputs and completed bakes on retry",
          "[almetal][utility-reload]")
{
    @autoreleasepool
    {
        Fixture f;
        auto owner = ALMetalTerrainPaintRenderer::create(*f.device, &f.error); REQUIRE(owner);
        const float positions[3][4]{{-1,-1,0,1},{3,-1,0,1},{-1,3,0,1}};
        const float uv[3][2]{{.5f,.5f},{.5f,.5f},{.5f,.5f}};
        std::array<std::uint8_t, sizeof(positions)+sizeof(uv)> data{};
        std::memcpy(data.data(), positions, sizeof(positions));
        std::memcpy(data.data()+sizeof(positions), uv, sizeof(uv));
        ALMetalBufferDescriptor bd; bd.length = data.size(); bd.storage = ALMetalStorageMode::shared;
        auto vertices = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, data.data(), data.size(), &f.error));
        const std::array<std::uint16_t, 3> ids{0,1,2}; bd.length = sizeof(ids);
        auto indices = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(indices);
        REQUIRE(indices->write(0, ids.data(), sizeof(ids), &f.error));
        auto alpha = f.texture(ALMetalPixelFormat::rgba8_unorm, 1, 1, false);
        auto destination = f.texture(ALMetalPixelFormat::rgba8_unorm, 8, 8, false, 4);
        const std::array<std::uint8_t, 4> ramp{0,0,0,128};
        auto upload = f.command();
        REQUIRE(alpha->upload(*upload, 0, 0, {}, ramp.data(), 4, &f.error));
        REQUIRE(upload->commit(&f.error));
        ALMetalTerrainPaintBatch batch;
        batch.vertices = vertices.get(); batch.texcoord_offset = sizeof(positions);
        batch.indices = indices.get(); batch.index_count = 3;
        batch.alpha_ramp = alpha.get(); batch.destination_template = destination.get();
        for (unsigned i = 0; i < 4; ++i) batch.modelview_projection[i*4+i] = 1;
        auto old_bake = owner->render(batch, &f.error); REQUIRE(old_bake);
        const auto generation = owner->shaderReloadGeneration();
        const auto alpha_handle = alpha->nativeHandle(), template_handle = destination->nativeHandle();
        ALMetalShaderLibrary::requestBuiltinReload();
        {
            PipelineFailure failure(*f.device);
            CHECK_FALSE(owner->render(batch, &f.error));
            CHECK(failure.calls == 1); f.checkInjected();
            CHECK(owner->shaderReloadGeneration() == generation);
        }
        auto bake = owner->render(batch, &f.error); REQUIRE(bake);
        CHECK(owner->shaderReloadGeneration() == generation + 1);
        CHECK(alpha->nativeHandle() == alpha_handle); CHECK(destination->nativeHandle() == template_handle);
        CHECK(f.read(*alpha) == std::vector<std::uint8_t>(ramp.begin(), ramp.end()));
        for (unsigned level = 0; level < 4; ++level)
        {
            const auto old_pixels = f.read(*old_bake, 4, level), pixels = f.read(*bake, 4, level);
            CHECK(old_pixels == pixels);
            for (std::size_t i = 0; i < pixels.size(); i += 4)
            { CHECK(pixels[i] == 64); CHECK(pixels[i+1] == 64); CHECK(pixels[i+2] == 63); CHECK(pixels[i+3] == 255); }
        }
    }
}

TEST_CASE("almetal: retained background shader retry preserves leases and frame history",
          "[almetal][utility-reload]")
{
    @autoreleasepool
    {
        Fixture f;
        ALMetalRetainedBackground owner;
        ALMetalRetainedBackground::Context context{f.device.get(), &f, 1, 8, 8, false};
        ALMetalRenderTargetDescriptor d; d.width = d.height = 4;
        d.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        d.depth_stencil_shader_read = true;
        std::shared_ptr<ALMetalRenderTarget> source = ALMetalRenderTarget::create(*f.device, d, &f.error);
        REQUIRE(source);
        auto clear = [&](bool green)
        {
            ALMetalRenderPassConfig pc;
            pc.colors[0].clear = green ? ALMetalClearColor{0,1,0,1} : ALMetalClearColor{1,0,0,1};
            pc.depth_store = pc.stencil_store = ALMetalStoreAction::store;
            pc.clear_depth = green ? .75 : .375;
            auto pass = source->makeRenderPass(pc, &f.error); REQUIRE(pass);
            auto command = f.command();
            auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Reload source clear", &f.error);
            REQUIRE(encoder); REQUIRE(encoder->end(&f.error)); REQUIRE(command->commit(&f.error));
        };
        clear(false); REQUIRE(owner.publish(context, 17, source, source, &f.error));
        auto lease = owner.find(context); REQUIRE(lease);
        const auto old_handle = lease->colorTexture()->nativeHandle();
        const auto generation = owner.shaderReloadGeneration();
        ALMetalShaderLibrary::requestBuiltinReload();
        CHECK(owner.find(context) == lease); CHECK(lease->source_frame == 17);
        clear(true);
        {
            PipelineFailure failure(*f.device);
            CHECK_FALSE(owner.publish(context, 18, source, source, &f.error));
            CHECK(failure.calls == 1); f.checkInjected();
            CHECK(owner.shaderReloadGeneration() == generation);
            CHECK(owner.find(context) == lease);
        }
        REQUIRE(owner.publish(context, 18, source, source, &f.error));
        auto current = owner.find(context); REQUIRE(current);
        CHECK(current != lease); CHECK(current->source_frame == 18);
        CHECK(lease->source_frame == 17); CHECK(lease->colorTexture()->nativeHandle() == old_handle);
        CHECK(owner.shaderReloadGeneration() == generation + 1);
        source.reset(); owner.clear();
        const auto old_pixels = f.read(*lease->colorTexture()), pixels = f.read(*current->colorTexture());
        for (std::size_t i = 0; i < pixels.size(); i += 4)
        {
            CHECK(old_pixels[i] == 0); CHECK(old_pixels[i+1] == 0);
            CHECK(old_pixels[i+2] == 255); CHECK(old_pixels[i+3] == 255);
            CHECK(pixels[i] == 0); CHECK(pixels[i+1] == 255);
            CHECK(pixels[i+2] == 0); CHECK(pixels[i+3] == 255);
        }
        std::array<float, 64> old_depth{}, depth{};
        REQUIRE(lease->depthTexture()->readback(*f.device, 0, 0, {}, old_depth.data(), 8*sizeof(float), &f.error));
        REQUIRE(current->depthTexture()->readback(*f.device, 0, 0, {}, depth.data(), 8*sizeof(float), &f.error));
        for (unsigned i = 0; i < 64; ++i) { CHECK(old_depth[i] == .375f); CHECK(depth[i] == .75f); }
    }
}
