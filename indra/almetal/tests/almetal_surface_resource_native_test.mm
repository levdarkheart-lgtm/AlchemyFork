/**
 * @file almetal_surface_resource_native_test.mm
 * @brief Actual CAMetalLayer and frame-scheduler typed backing ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetaldevice.h"
#include "../almetalframe.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include "../almetalsurface.h"
#include "../almetaluirenderer.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace surface_resource
{
namespace RP = ALMetalResourcePlanning;

template<class Predicate> bool eventually(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

void requireValidation()
{
    const char* api = std::getenv("MTL_DEBUG_LAYER");
    const char* shader = std::getenv("MTL_SHADER_VALIDATION");
    REQUIRE(api);
    REQUIRE(shader);
    REQUIRE(std::string(api) == "1");
    REQUIRE(std::string(shader) == "1");
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> scheduler;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;

    Fixture()
    {
        requireValidation();
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled);
        INFO(error); REQUIRE(device);
        REQUIRE(device->resourcePlanningMode() == ALMetalResourcePlanMode::enabled);
        scheduler = ALMetalFrameScheduler::create(*device, 1, &error);
        INFO(error); REQUIRE(scheduler);
        library = ALMetalShaderLibrary::compileSource(*device, R"metal(
            #include <metal_stdlib>
            using namespace metal;
            struct Raster { float4 position [[position]]; };
            vertex Raster surface_resource_vertex(uint id [[vertex_id]])
            {
                const float2 p[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
                return {float4(p[id], 0, 1)};
            }
            fragment float4 surface_resource_fragment(Raster input [[stage_in]])
            {
                return (uint(input.position.x) & 1u)
                    ? float4(0,1,1,1) : float4(1,0,1,1);
            }
        )metal", "Contained surface ownership test", &error);
        INFO(error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "surface_resource_vertex";
        descriptor.fragment_function = "surface_resource_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        descriptor.label = "Contained surface exact pattern";
        pipeline = ALMetalRenderPipeline::create(*device, descriptor, &error);
        INFO(error); REQUIRE(pipeline);
        std::cout << "surface_resource_shader_validation=" << pipeline->shaderValidationMode() << '\n';
        REQUIRE(pipeline->shaderValidationMode() == static_cast<int>(MTLShaderValidationEnabled));
    }

    void draw(ALMetalCommandBuffer& command, ALMetalRenderPass& pass,
              std::uint32_t width, std::uint32_t height)
    {
        auto encoder = ALMetalRenderEncoder::begin(command, pass, "Contained surface pattern", &error);
        INFO(error); REQUIRE(encoder);
        REQUIRE(encoder->setViewport({0, 0, static_cast<double>(width), static_cast<double>(height), 0, 1}, &error));
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error));
    }

    void completed(ALMetalFrame& frame, const std::shared_ptr<RP::Command>& graph)
    {
        REQUIRE(graph);
        REQUIRE_FALSE(graph->stats().opaque);
        REQUIRE(scheduler->submit(frame, &error));
        INFO(error);
        // This catches the old FrameScheduler nativeHandle() observer: that
        // accessor made an otherwise completely typed command opaque.
        CHECK_FALSE(graph->stats().opaque);
        REQUIRE(scheduler->waitUntilIdle(2000, &error));
        INFO(error);
        REQUIRE(frame.commandBuffer().status() == ALMetalCommandStatus::completed);
        CHECK(eventually([&] { return graph->stats().retired; }));
        CHECK(device->uniqueResourceStats().uncommitted_uses == 0);
        CHECK(device->uniqueResourceStats().submitted_uses == 0);
        CHECK(scheduler->stats().failed == 0);
    }
};

void pattern(const std::uint8_t* data, std::size_t row_bytes,
             std::uint32_t width, std::uint32_t height)
{
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto* pixel = data + y * row_bytes + x * 4;
            CHECK(pixel[0] == 255); // BGRA8: both authored colors contain blue.
            CHECK(pixel[1] == ((x & 1u) ? 255 : 0));
            CHECK(pixel[2] == ((x & 1u) ? 0 : 255));
            CHECK(pixel[3] == 255);
        }
}

struct WindowDeleter
{
    void operator()(SDL_Window* window) const { if (window) SDL_DestroyWindow(window); }
};

struct VideoScope
{
    bool owns_video = false;
    VideoScope()
    {
        REQUIRE([NSThread isMainThread]);
        if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
        {
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
                SKIP("Actual surface unavailable: SDL video initialization: " << SDL_GetError());
            owns_video = true;
        }
    }
    ~VideoScope() { if (owns_video) SDL_QuitSubSystem(SDL_INIT_VIDEO); }
};

struct Platform
{
    // Construct first/destruct last, including a constructor's skip/exception.
    VideoScope video;
    std::unique_ptr<SDL_Window, WindowDeleter> window;
    std::unique_ptr<ALMetalSurface> surface;

    explicit Platform(Fixture& f)
    {
        SDL_PropertiesID properties = SDL_CreateProperties();
        REQUIRE(properties != 0);
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Alchemy contained Metal ownership test");
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 64);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 48);
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        window.reset(SDL_CreateWindowWithProperties(properties));
        SDL_DestroyProperties(properties);
        if (!window) SKIP("Actual surface unavailable: SDL window: " << SDL_GetError());
        ALMetalSurfaceConfig config;
        config.framebuffer_only = false; // Permit the independent GPU pixel readback below.
        surface = ALMetalSurface::create(window.get(), *f.device, config, &f.error);
        INFO(f.error); REQUIRE(surface);
        REQUIRE(surface->resize(64, 48, &f.error));
        CHECK((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_OPENGL) == 0);
        CHECK_FALSE(surface->info().framebuffer_only);
    }
    ~Platform()
    {
        surface.reset(); // The Metal overlay must die before its SDL/Cocoa window.
        window.reset();
    }
    std::unique_ptr<ALMetalDrawable> acquire(Fixture& f)
    {
        auto drawable = surface->nextDrawable(&f.error);
        if (!drawable)
        {
            REQUIRE(SDL_ShowWindow(window.get()));
            REQUIRE(SDL_SyncWindow(window.get()));
            SDL_PumpEvents();
            drawable = surface->nextDrawable(&f.error);
        }
        if (!drawable) SKIP("Actual CAMetalLayer drawable unavailable after showing contained window: " << f.error);
        REQUIRE(drawable->width() == surface->info().width);
        REQUIRE(drawable->height() == surface->info().height);
        return drawable;
    }
};

std::unique_ptr<ALMetalBuffer> copyDrawable(
    Fixture& f, ALMetalCommandBuffer& command, ALMetalDrawable& drawable, std::size_t row_bytes)
{
    ALMetalBufferDescriptor descriptor;
    descriptor.length = row_bytes * drawable.height();
    descriptor.label = "Contained actual drawable readback";
    auto buffer = ALMetalBuffer::create(*f.device, descriptor, &f.error);
    INFO(f.error); REQUIRE(buffer);
    std::uint64_t pass = 0;
    REQUIRE(command.beginResourcePass(RP::Pass::blit, pass, &f.error));
    REQUIRE(command.useResource(drawable.resourceView(), RP::Access::read, &f.error));
    REQUIRE(command.useResource(buffer->resourceView(), RP::Access::write, &f.error));
    id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command.nativeHandleForDeclaredResources();
    REQUIRE(native);
    id<MTLBlitCommandEncoder> blit = [native blitCommandEncoder];
    REQUIRE(blit);
    [blit copyFromTexture:(__bridge id<MTLTexture>)drawable.nativeTexture()
             sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
              sourceSize:MTLSizeMake(drawable.width(), drawable.height(), 1)
                toBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle()
       destinationOffset:0 destinationBytesPerRow:row_bytes
     destinationBytesPerImage:descriptor.length];
    [blit endEncoding];
    REQUIRE(command.endResourcePass(pass, &f.error));
    return buffer;
}

ALMetalUIBatch greenBatch(ALMetalBuffer& vertices, std::uint32_t width, std::uint32_t height)
{
    ALMetalUIBatch batch;
    batch.vertices = &vertices;
    batch.vertex_count = 3;
    batch.texcoord_offset = 3 * 4 * sizeof(float);
    batch.color_offset = batch.texcoord_offset + 3 * 2 * sizeof(float);
    batch.shader = ALMetalUIShader::vertex_color;
    batch.blend.enabled = false;
    batch.target_width = width;
    batch.target_height = height;
    batch.viewport = {0, 0, static_cast<double>(width), static_cast<double>(height), 0, 1};
    batch.scissor = {0, 0, width, height};
    for (unsigned i : {0u, 5u, 10u, 15u})
        batch.matrices.modelview[i] = batch.matrices.projection[i] =
            batch.matrices.modelview_projection[i] = batch.matrices.inverse_projection[i] =
            batch.matrices.texture0[i] = 1.f;
    return batch;
}
}

TEST_CASE("Native frame scheduler preserves typed scope and retires real offscreen work",
          "[surface-resource][surface-resource-offscreen]")
{
    @autoreleasepool
    {
        using namespace surface_resource;
        Fixture f;
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = 8;
        descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        auto target = ALMetalRenderTarget::create(*f.device, descriptor, &f.error);
        INFO(f.error); REQUIRE(target);
        auto frame = f.scheduler->beginFrame("Contained typed frame", 1000, &f.error);
        INFO(f.error); REQUIRE(frame);
        auto graph = frame->commandBuffer().resourceUsePlan(); REQUIRE(graph);
        auto pass = target->makeRenderPass({}, &f.error); REQUIRE(pass);
        f.draw(frame->commandBuffer(), *pass, 8, 8);
        CHECK(graph->stats().resources == 1);
        CHECK(graph->stats().passes == 1);
        CHECK(f.device->uniqueResourceStats().uncommitted_uses == 1);
        f.completed(*frame, graph);
        std::array<std::uint8_t, 8 * 8 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), 8 * 4, &f.error));
        pattern(pixels.data(), 8 * 4, 8, 8);

        frame.reset();
        auto abandoned = f.scheduler->beginFrame("Contained abandoned typed frame", 1000, &f.error);
        REQUIRE(abandoned);
        auto abandoned_graph = abandoned->commandBuffer().resourceUsePlan();
        f.draw(abandoned->commandBuffer(), *pass, 8, 8);
        CHECK_FALSE(abandoned_graph->stats().opaque);
        const auto before = f.device->uniqueResourceStats().abandoned_commands;
        abandoned.reset();
        CHECK(abandoned_graph->stats().retired);
        CHECK(f.device->uniqueResourceStats().abandoned_commands == before + 1);
        CHECK(f.device->uniqueResourceStats().uncommitted_uses == 0);
        auto next = f.scheduler->beginFrame("Contained slot after abandonment", 1000, &f.error);
        REQUIRE(next);
        auto next_graph = next->commandBuffer().resourceUsePlan();
        f.draw(next->commandBuffer(), *pass, 8, 8);
        f.completed(*next, next_graph);
        CHECK(f.scheduler->stats().submitted == 2);
        CHECK(f.scheduler->stats().completed == 2);
    }
}

TEST_CASE("Actual drawable rendering readback and presentation share one imported backing",
          "[surface-resource][surface-resource-platform]")
{
    @autoreleasepool
    {
        using namespace surface_resource;
        Fixture f;
        Platform platform(f);
        auto drawable = platform.acquire(f);
        REQUIRE(drawable->prepareResourceUse(*f.device, &f.error));
        const auto identity = drawable->resourceView().identity();
        REQUIRE(identity != 0);
        CHECK(drawable->resourceView().nativeIdentity() == drawable->nativeTexture());
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 1);
        CHECK(f.device->uniqueResourceStats().owned_backings == 0);
        REQUIRE(drawable->prepareResourceUse(*f.device, &f.error));
        CHECK(drawable->resourceView().identity() == identity);
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 1);
        // Wrappers of one native GPU and planning mode deliberately share the
        // canonical execution owner and registry. They are valid consumers.
        auto same_owner = ALMetalDevice::create(&f.error, ALMetalResourcePlanMode::enabled);
        REQUIRE(same_owner);
        REQUIRE(same_owner->resourcePlan() == f.device->resourcePlan());
        REQUIRE(drawable->prepareResourceUse(*same_owner, &f.error));
        // A different mode owns an independent registry. Keep the substantive
        // negative ownership check against that genuinely different plan.
        auto other_device = ALMetalDevice::create(&f.error, ALMetalResourcePlanMode::accounting_only);
        REQUIRE(other_device);
        REQUIRE(other_device->resourcePlan() != f.device->resourcePlan());
        CHECK_FALSE(drawable->prepareResourceUse(*other_device, &f.error));
        CHECK(drawable->resourceView().identity() == identity);
        f.error.clear();

        auto frame = f.scheduler->beginFrame("Contained actual drawable frame", 1000, &f.error);
        REQUIRE(frame);
        auto graph = frame->commandBuffer().resourceUsePlan(); REQUIRE(graph);
        auto pass = ALMetalRenderPass::createForColorAttachment(*f.device,
            drawable->nativeTexture(), ALMetalPixelFormat::bgra8_unorm,
            {}, drawable->resourceView(), &f.error);
        INFO(f.error); REQUIRE(pass);
        f.draw(frame->commandBuffer(), *pass, drawable->width(), drawable->height());
        const std::size_t row_bytes = (static_cast<std::size_t>(drawable->width()) * 4 + 255) & ~std::size_t(255);
        auto readback = copyDrawable(f, frame->commandBuffer(), *drawable, row_bytes);
        REQUIRE(platform.surface->present(frame->commandBuffer(), *drawable, &f.error));
        CHECK(drawable->wasPresented());
        CHECK_FALSE(platform.surface->present(frame->commandBuffer(), *drawable, &f.error));
        CHECK_FALSE(drawable->prepareResourceUse(*f.device, &f.error));
        CHECK(graph->stats().resources == 2); // One drawable identity plus readback buffer.
        CHECK(graph->stats().passes == 3); // Color, declared copy, present/read.
        CHECK(graph->stats().read_resources == 1);
        CHECK(graph->stats().write_resources == 2);
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 1);
        f.completed(*frame, graph);
        std::vector<std::uint8_t> pixels(row_bytes * drawable->height());
        REQUIRE(readback->read(0, pixels.data(), pixels.size(), &f.error));
        pattern(pixels.data(), row_bytes, drawable->width(), drawable->height());
        pass.reset(); drawable.reset(); readback.reset(); frame.reset();
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);
        CHECK(f.device->uniqueResourceStats().owned_backings == 0);
    }
}

TEST_CASE("Actual drawable abandonment and resize do not retain a reader or exhaust frame slots",
          "[surface-resource][surface-resource-platform]")
{
    @autoreleasepool
    {
        using namespace surface_resource;
        Fixture f;
        Platform platform(f);
        auto drawable = platform.acquire(f);
        REQUIRE(drawable->prepareResourceUse(*f.device, &f.error));
        auto frame = f.scheduler->beginFrame("Contained unpresented abandonment", 1000, &f.error);
        REQUIRE(frame);
        auto graph = frame->commandBuffer().resourceUsePlan();
        auto pass = ALMetalRenderPass::createForColorAttachment(*f.device,
            drawable->nativeTexture(), ALMetalPixelFormat::bgra8_unorm,
            {}, drawable->resourceView(), &f.error);
        REQUIRE(pass);
        f.draw(frame->commandBuffer(), *pass, drawable->width(), drawable->height());
        CHECK_FALSE(drawable->wasPresented());
        CHECK(f.device->uniqueResourceStats().uncommitted_uses == 1);
        frame.reset();
        CHECK(graph->stats().retired);
        CHECK(f.device->uniqueResourceStats().uncommitted_uses == 0);
        pass.reset(); drawable.reset();
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);

        auto old = platform.acquire(f);
        const auto old_generation = old->generation();
        REQUIRE(platform.surface->resize(80, 52, &f.error));
        CHECK(platform.surface->info().generation == old_generation + 1);
        auto next_frame = f.scheduler->beginFrame("Contained resized drawable frame", 1000, &f.error);
        REQUIRE(next_frame);
        auto next_graph = next_frame->commandBuffer().resourceUsePlan();
        CHECK_FALSE(platform.surface->present(next_frame->commandBuffer(), *old, &f.error));
        CHECK(next_graph->stats().resources == 0);
        CHECK(next_graph->stats().passes == 0);
        CHECK_FALSE(next_graph->stats().opaque);
        old.reset();
        auto next = platform.acquire(f);
        REQUIRE(next->generation() == platform.surface->info().generation);
        REQUIRE(next->prepareResourceUse(*f.device, &f.error));
        auto next_pass = ALMetalRenderPass::createForColorAttachment(*f.device,
            next->nativeTexture(), ALMetalPixelFormat::bgra8_unorm,
            {}, next->resourceView(), &f.error);
        REQUIRE(next_pass);
        f.draw(next_frame->commandBuffer(), *next_pass, next->width(), next->height());
        REQUIRE(platform.surface->present(next_frame->commandBuffer(), *next, &f.error));
        f.completed(*next_frame, next_graph);
        CHECK(next_graph->stats().resources == 1);
        CHECK(next_graph->stats().passes == 2);
        CHECK(f.scheduler->stats().submitted == 1);
        next_pass.reset(); next.reset(); next_frame.reset();
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);
    }
}

TEST_CASE("Production UI direct and fused surface paths complete typed backing ownership",
          "[surface-resource][surface-resource-platform][surface-resource-ui]")
{
    @autoreleasepool
    {
        using namespace surface_resource;
        Fixture f;
        Platform platform(f);
        // Force the same hidden-window availability check used by the direct
        // drawable tests before production UI owns acquisition internally.
        platform.acquire(f).reset();
        constexpr std::size_t uv_offset = 3 * 4 * sizeof(float);
        constexpr std::size_t color_offset = uv_offset + 3 * 2 * sizeof(float);
        std::array<std::uint8_t, color_offset + 3 * 4> bytes{};
        const std::array<float, 12> positions{-1,-1,0,1, 3,-1,0,1, -1,3,0,1};
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        for (unsigned i = 0; i < 3; ++i)
        { bytes[color_offset + i * 4 + 1] = 255; bytes[color_offset + i * 4 + 3] = 255; }
        ALMetalBufferDescriptor descriptor;
        descriptor.length = bytes.size();
        auto vertices = ALMetalBuffer::create(*f.device, descriptor, &f.error);
        REQUIRE(vertices);
        REQUIRE(vertices->write(0, bytes.data(), bytes.size(), &f.error));
        auto batch = greenBatch(*vertices, platform.surface->info().width, platform.surface->info().height);
        auto direct = ALMetalUIRenderer::createForSurface(*f.device, *f.scheduler, *platform.surface, &f.error);
        INFO(f.error); REQUIRE(direct);
        REQUIRE(direct->draw(batch, &f.error));
        REQUIRE(direct->endFrame(&f.error));
        REQUIRE(f.scheduler->waitUntilIdle(2000, &f.error));
        CHECK(direct->stats().frames == 1);
        CHECK(direct->stats().batches == 1);
        CHECK(f.scheduler->stats().submitted == 1);
        CHECK(f.scheduler->stats().failed == 0);
        CHECK(f.device->uniqueResourceStats().opaque_uncommitted_commands == 0);
        CHECK(eventually([&] { return f.device->uniqueResourceStats().submitted_uses == 0; }));
        direct.reset();
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);

        auto fused = ALMetalUIRenderer::createForTexture(*f.device, *f.scheduler, ALMetalPixelFormat::bgra8_unorm, &f.error);
        INFO(f.error); REQUIRE(fused);
        REQUIRE(fused->draw(batch, &f.error));
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(fused->endFrameAndPresent(*platform.surface, &f.error));
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions + 1);
        auto output = fused->takeOutputTexture(); REQUIRE(output);
        REQUIRE(f.scheduler->waitUntilIdle(2000, &f.error));
        CHECK(f.scheduler->stats().submitted == 2);
        CHECK(f.scheduler->stats().failed == 0);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(batch.target_width) * batch.target_height * 4);
        REQUIRE(output->readback(*f.device, 0, 0, {}, pixels.data(), batch.target_width * 4, &f.error));
        for (std::size_t i = 0; i < pixels.size(); i += 4)
        { CHECK(pixels[i] == 0); CHECK(pixels[i + 1] == 255); CHECK(pixels[i + 2] == 0); CHECK(pixels[i + 3] == 255); }
        fused.reset();
        CHECK(eventually([&] { return f.device->uniqueResourceStats().submitted_uses == 0; }));
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);
        CHECK(f.device->uniqueResourceStats().uncommitted_uses == 0);
    }
}
