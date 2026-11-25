/**
 * @file almetal_temporal_presentation_test.mm
 * @brief Production Temporal HDR, glow-alpha, post-processing and transaction checks.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "../almetalworld.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
using Pixel = std::array<float, 4>;
using Bytes = std::array<std::uint8_t, 4>;
constexpr unsigned IW = 64, IH = 48, OW = 128, OH = 96;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalTemporalScaler> scaler;
    std::shared_ptr<ALMetalWorldFrameData> frame_data;
    ALMetalTemporalScalerDescriptor desc;
    std::unique_ptr<ALMetalRenderTarget> source, depth, motion, reconstructed, resolved;
    Fixture()
    {
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled); REQUIRE(device); INFO(error);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error); REQUIRE(renderer); INFO(error);
        frame_data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frame_data);
        frame_data->beginFrame(1); renderer->setFrameData(frame_data);
        scaler = ALMetalTemporalScaler::create(*device, &error); REQUIRE(scaler); INFO(error);
        desc.input_width = IW; desc.input_height = IH; desc.output_width = OW; desc.output_height = OH;
        ALMetalTemporalScalerRequirements req;
        REQUIRE(scaler->prepare(desc, req, &error)); INFO(error);
        source = target(IW, IH, ALMetalPixelFormat::rgba16_float);
        ALMetalRenderTargetDescriptor dd;
        dd.width = IW; dd.height = IH; dd.color_attachment_count = 0;
        dd.depth_stencil_format = ALMetalPixelFormat::depth32_float; dd.depth_stencil_shader_read = true;
        depth = ALMetalRenderTarget::create(*device, dd, &error); REQUIRE(depth);
        ALMetalRenderTargetDescriptor md;
        md.width = IW; md.height = IH; md.color_attachment_count = 3;
        md.colors[0].format = ALMetalPixelFormat::rg16_float;
        md.colors[1].format = ALMetalPixelFormat::r8_unorm;
        md.colors[2].format = ALMetalPixelFormat::r32_float;
        motion = ALMetalRenderTarget::create(*device, md, &error); REQUIRE(motion);
        reconstructed = renderer->prepareTemporalTarget(OW, OH, true, &error); REQUIRE(reconstructed); INFO(error);
        resolved = renderer->prepareTemporalTarget(OW, OH, false, &error); REQUIRE(resolved); INFO(error);
        auto upload = device->makeCommandBuffer("Temporal presentation authored HDR inputs", &error); REQUIRE(upload);
        std::vector<__fp16> pixels(IW * IH * 4), vectors(IW * IH * 2, 0.f);
        std::vector<float> devices(IW * IH, .75f);
        std::vector<std::uint8_t> masks(IW * IH, 0);
        for (unsigned y = 0; y < IH; ++y) for (unsigned x = 0; x < IW; ++x)
        {
            auto* p = pixels.data() + (y * IW + x) * 4;
            p[0] = x < IW / 2 ? .125f : 4.f;
            p[1] = y < IH / 2 ? .25f : 2.f;
            p[2] = (x + y) % 3 ? .5f : 1.5f;
            p[3] = .125f + .75f * float(y) / float(IH - 1);
        }
        REQUIRE(const_cast<ALMetalTexture*>(source->colorTexture(0))->upload(*upload, 0, 0, {}, pixels.data(), IW * 8, &error));
        REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(0))->upload(*upload, 0, 0, {}, vectors.data(), IW * 4, &error));
        REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(1))->upload(*upload, 0, 0, {}, masks.data(), IW, &error));
        REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(2))->upload(*upload, 0, 0, {}, devices.data(), IW * 4, &error));
        ALMetalRenderPassConfig clear; clear.clear_depth = 0.; clear.depth_store = ALMetalStoreAction::store;
        auto pass = depth->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*upload, *pass, "Temporal authored reversed far depth", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(upload->commitAndWait(&error)); INFO(error);
    }
    ~Fixture()
    {
        if (renderer) renderer->cancelCommandBatch();
        if (device) device->waitUntilIdle();
    }
    std::unique_ptr<ALMetalRenderTarget> target(unsigned width, unsigned height, ALMetalPixelFormat format)
    {
        ALMetalRenderTargetDescriptor td; td.width = width; td.height = height; td.colors[0].format = format;
        auto result = ALMetalRenderTarget::create(*device, td, &error); REQUIRE(result); INFO(error); return result;
    }
    std::vector<Pixel> readHDR(const ALMetalRenderTarget& target)
    {
        const auto width = target.descriptor().width, height = target.descriptor().height;
        std::vector<__fp16> raw(std::size_t(width) * height * 4);
        const bool ok = target.colorTexture(0)->readback(*device, 0, 0, {}, raw.data(), width * 8, &error);
        INFO(error); REQUIRE(ok);
        std::vector<Pixel> output(raw.size() / 4);
        for (std::size_t i = 0; i < raw.size(); ++i) output[i / 4][i % 4] = raw[i];
        return output;
    }
    std::vector<Bytes> readBytes(const ALMetalRenderTarget& target)
    {
        std::vector<Bytes> result(std::size_t(target.descriptor().width) * target.descriptor().height);
        const bool ok = target.colorTexture(0)->readback(*device, 0, 0, {}, result.data(), target.descriptor().width * 4, &error);
        INFO(error); REQUIRE(ok); return result;
    }
    std::unique_ptr<ALMetalRenderTarget> dof()
    {
        ALMetalWorldDepthOfFieldScene scene;
        scene.source = source.get(); scene.depth_texture = depth->depthStencilTexture();
        scene.inverse_projection[0] = scene.inverse_projection[5] = scene.inverse_projection[15] = 1.f;
        scene.inverse_projection[10] = 2.f; scene.inverse_projection[14] = -3.f;
        scene.focal_distance = -.5f; scene.blur_constant = 24.f / 1.414f;
        scene.tan_pixel_angle = scene.magnification = 1.f; scene.max_cof = 4.f;
        scene.rgba16_blur = true; scene.reverse_z = true;
        auto result = renderer->renderDepthOfField(scene, &error); REQUIRE(result); INFO(error); return result;
    }
    ALMetalTemporalScaler::Ticket temporal(const ALMetalRenderTarget& input, float jitter = 0.f)
    {
        ALMetalTemporalScalerFrame frame; frame.jitter_x = jitter;
        ALMetalTemporalScaler::Ticket ticket = 0;
        const bool encoded = renderer->encodeTemporalUpscale(*scaler, desc, *input.colorTexture(0), *motion,
            *reconstructed, frame, ticket, &error);
        INFO(error); REQUIRE(encoded); REQUIRE(ticket);
        REQUIRE(renderer->resolveTemporalColor(input, *reconstructed, *resolved, &error)); INFO(error);
        return ticket;
    }
    std::unique_ptr<ALMetalRenderTarget> display(const ALMetalTexture* adapted = nullptr)
    {
        ALMetalWorldDisplayPostScene scene; scene.source = resolved.get(); scene.hdr = true;
        scene.adapted_exposure = .25f; scene.adapted_exposure_texture = adapted;
        auto result = renderer->renderDisplayPost(scene, &error); REQUIRE(result); INFO(error); return result;
    }
    std::unique_ptr<ALMetalRenderTarget> present(const ALMetalRenderTarget& input, bool invalid = false)
    {
        ALMetalWorldPresentationScene scene; scene.source = &input; scene.output_width = OW; scene.output_height = OH;
        if (invalid) scene.lens_distortion_amount = -1.f;
        return renderer->renderPresentationPost(scene, &error);
    }
    struct Settlement { unsigned published = 0, rejected = 0; bool history_submitted = false; };
    std::shared_ptr<Settlement> observe(ALMetalTemporalScaler::Ticket ticket)
    {
        auto result = std::make_shared<Settlement>();
        REQUIRE(renderer->observeCommandBatchSettlement([this, ticket, result](bool committed)
        {
            if (committed) { ++result->published; result->history_submitted = scaler->submitted(ticket, &error); }
            else { ++result->rejected; scaler->abandon(ticket); }
        }, &error));
        return result;
    }
};

struct PendingGate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<unsigned>> state = std::make_shared<std::atomic<unsigned>>(0);
    explicit PendingGate(Fixture& f)
    {
        event = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent]; REQUIRE(event);
        auto command = f.device->makeCommandBuffer("Temporal pending-world queue gate", &f.error); REQUIRE(command);
        [(__bridge id<MTLCommandBuffer>)command->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(command->commit(&f.error));
        auto observed = state; id<MTLSharedEvent> retained = event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                unsigned expected = 0;
                if (observed->compare_exchange_strong(expected, 2)) retained.signaledValue = 1;
            });
    }
    void release() { unsigned expected = 0; state->compare_exchange_strong(expected, 1); event.signaledValue = 1; }
    ~PendingGate() { release(); }
};
}

TEST_CASE("Temporal preserves HDR glow alpha and native post dimensions after reduced-depth DoF", "[metal][temporal-presentation]")
{
    @autoreleasepool
    {
        Fixture f;
        auto exposure = f.target(1, 1, ALMetalPixelFormat::rgba16_float);
        auto upload = f.device->makeCommandBuffer("Temporal downstream exposure", &f.error); REQUIRE(upload);
        const std::array<__fp16, 4> scale{.25f,0.f,0.f,0.f};
        REQUIRE(const_cast<ALMetalTexture*>(exposure->colorTexture(0))->upload(*upload,0,0,{},scale.data(),8,&f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(f.renderer->beginMainFrameBatch(1, "Temporal HDR and downstream post", &f.error));
        auto defocused = f.dof();
        REQUIRE(defocused->descriptor().width == IW); REQUIRE(defocused->descriptor().height == IH);
        auto ticket = f.temporal(*defocused);
        auto scalar_post = f.display(); auto gpu_post = f.display(exposure->colorTexture(0));
        REQUIRE(scalar_post->descriptor().width == OW); REQUIRE(scalar_post->descriptor().height == OH);
        auto final = f.present(*gpu_post); REQUIRE(final); INFO(f.error);
        REQUIRE(final->descriptor().width == OW); REQUIRE(final->descriptor().height == OH);
        REQUIRE(ALMetalDevice::committedCommandBufferCount() == submissions);
        auto settled = f.observe(ticket);
        REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settled->history_submitted);
        REQUIRE(settled->published == 1); REQUIRE(settled->rejected == 0);
        // Scope sampling is an independently submitted existing consumer; it
        // must follow submission of the world that produces its source.
        auto scope = f.renderer->renderDisplaySample(*gpu_post, OW / 4, OH / 4, &f.error); REQUIRE(scope); INFO(f.error);
        REQUIRE(scope->descriptor().width == OW / 4); REQUIRE(scope->descriptor().height == OH / 4);
        const auto resolved = f.readHDR(*f.resolved), reconstructed = f.readHDR(*f.reconstructed);
        const auto original_alpha = f.readHDR(*defocused);
        unsigned hdr_values = 0;
        for (unsigned y = 0; y < OH; ++y) for (unsigned x = 0; x < OW; ++x)
        {
            const auto& pixel = resolved[y * OW + x];
            for (unsigned c = 0; c < 3; ++c)
            { REQUIRE(std::isfinite(pixel[c])); REQUIRE(std::abs(pixel[c] - reconstructed[y * OW + x][c]) < .005f); }
            hdr_values += pixel[0] > 1.f;
            const float sy = (float(y) + .5f) * float(IH) / float(OH) - .5f;
            const int low = std::clamp(int(std::floor(sy)), 0, int(IH) - 1);
            const int high = std::clamp(int(std::floor(sy)) + 1, 0, int(IH) - 1);
            const float t = sy - std::floor(sy);
            const float expected_alpha = original_alpha[low * IW][3] * (1.f - t) + original_alpha[high * IW][3] * t;
            REQUIRE(std::abs(pixel[3] - expected_alpha) < .002f);
        }
        REQUIRE(hdr_values > OW * OH / 8);
        const auto scalar = f.readBytes(*scalar_post), gpu = f.readBytes(*gpu_post);
        REQUIRE(scalar.size() == gpu.size());
        for (std::size_t p = 0; p < scalar.size(); ++p) for (unsigned c = 0; c < 4; ++c)
            REQUIRE(std::abs(int(scalar[p][c]) - int(gpu[p][c])) <= 1);
        REQUIRE(f.scaler->stats().submitted_calls == 1);
    }
}

TEST_CASE("Temporal encoded prefix is abandoned when final presentation validation fails", "[metal][temporal-presentation]")
{
    @autoreleasepool
    {
        Fixture f;
        REQUIRE(f.renderer->beginMainFrameBatch(1, "Temporal accepted previous world", &f.error));
        auto first_ticket = f.temporal(*f.source); auto first_display = f.display();
        auto previous = f.present(*first_display); REQUIRE(previous);
        auto first_settlement = f.observe(first_ticket);
        REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(first_settlement->history_submitted);
        const auto previous_pixels = f.readBytes(*previous);
        const auto previous_backing = previous->colorTexture(0)->nativeHandle();
        const auto submitted = ALMetalDevice::committedCommandBufferCount();
        const auto resets = f.scaler->stats().reset_calls;
        f.frame_data->beginFrame(2);
        REQUIRE(f.renderer->beginMainFrameBatch(2, "Temporal rejected jittered next world", &f.error));
        const auto encoded_before = f.scaler->stats().encoded_calls;
        auto failed_ticket = f.temporal(*f.source, .25f);
        REQUIRE(f.scaler->stats().encoded_calls == encoded_before + 1);
        auto failed_display = f.display();
        auto settled = f.observe(failed_ticket);
        auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
        REQUIRE_FALSE(f.present(*failed_display, true)); REQUIRE_FALSE(f.error.empty());
        REQUIRE_FALSE(f.renderer->commitCommandBatch(&f.error));
        REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted);
        REQUIRE(settled->published == 0); REQUIRE(settled->rejected == 1);
        REQUIRE_FALSE(f.scaler->stats().history_valid);
        REQUIRE(f.scaler->stats().submitted_calls == 1); REQUIRE(f.scaler->stats().abandoned_calls == 1);
        REQUIRE(completion->isComplete()); REQUIRE(completion->status() == ALMetalCommandStatus::unknown);
        REQUIRE(previous->colorTexture(0)->nativeHandle() == previous_backing);
        REQUIRE(f.readBytes(*previous) == previous_pixels);
        f.frame_data->beginFrame(3);
        REQUIRE(f.renderer->beginMainFrameBatch(2, "Temporal reset after failed final presentation", &f.error));
        auto recovered_ticket = f.temporal(*f.source);
        auto recovered_display = f.display(); auto recovered = f.present(*recovered_display); REQUIRE(recovered);
        auto recovered_settlement = f.observe(recovered_ticket);
        REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(recovered_settlement->history_submitted);
        REQUIRE(f.scaler->stats().submitted_calls == 2); REQUIRE(f.scaler->stats().reset_calls == resets + 1);
        REQUIRE(f.readBytes(*recovered) == previous_pixels);
    }
}

TEST_CASE("Temporal worlds submit and retain their post resources while two groups remain pending", "[metal][temporal-presentation]")
{
    @autoreleasepool
    {
        Fixture f;
        PendingGate gate(f);
        std::array<std::shared_ptr<const ALMetalCommandCompletion>, 2> completion;
        std::array<std::shared_ptr<Fixture::Settlement>, 2> settlement;
        std::array<std::unique_ptr<ALMetalRenderTarget>, 2> outputs;
        const auto submitted = ALMetalDevice::committedCommandBufferCount();
        for (unsigned index = 0; index < 2; ++index)
        {
            f.frame_data->beginFrame(index + 1);
            REQUIRE(f.renderer->beginMainFrameBatch(index + 1, "Temporal pending required world", &f.error));
            auto ticket = f.temporal(*f.source); auto display = f.display();
            outputs[index] = f.present(*display); REQUIRE(outputs[index]);
            settlement[index] = f.observe(ticket); completion[index] = f.renderer->commandBatchCompletion();
            REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settlement[index]->history_submitted);
            REQUIRE(settlement[index]->published == 1);
            REQUIRE_FALSE(completion[index]->isComplete());
        }
        REQUIRE(gate.state->load() == 0);
        REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted + 2);
        REQUIRE(f.scaler->stats().submitted_calls == 2); REQUIRE(f.scaler->stats().reset_calls == 1);
        f.scaler.reset(); gate.release();
        for (const auto& token : completion) REQUIRE(token->waitUntilCompleted(&f.error));
        const auto first = f.readBytes(*outputs[0]), second = f.readBytes(*outputs[1]);
        // History is expected to refine the authored high-frequency blue
        // detail. Constant red/green interiors must remain stable; requiring
        // bit identity would incorrectly reject genuine history use.
        for (unsigned y = 16; y + 16 < OH; ++y) for (unsigned x = 16; x + 16 < OW; ++x)
        {
            if (std::abs(int(x) - int(OW / 2)) < 16 || std::abs(int(y) - int(OH / 2)) < 16) continue;
            for (unsigned c : {1u, 2u})
                REQUIRE(std::abs(int(first[y * OW + x][c]) - int(second[y * OW + x][c])) <= 2);
        }
    }
}

TEST_CASE("Temporal repeated post transactions keep scaler and pooled ownership bounded", "[metal][temporal-presentation]")
{
    const auto before = alMetalResourceAllocationStats();
    {
        @autoreleasepool
        {
            Fixture f;
            const auto created = alMetalResourceAllocationStats().textures_created;
            std::uint64_t warm_created = created;
            for (unsigned index = 0; index < 12; ++index)
            {
                f.frame_data->beginFrame(index + 1);
                REQUIRE(f.renderer->beginMainFrameBatch(index + 1, "Temporal bounded pool sequence", &f.error));
                auto ticket = f.temporal(*f.source);
                auto display = f.display(); auto final = f.present(*display); REQUIRE(final);
                auto settled = f.observe(ticket);
                auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
                REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settled->history_submitted);
                // This exceptional offscreen test settles each transaction to
                // examine retained pool storage. Production gains no new wait.
                REQUIRE(completion->waitUntilCompleted(&f.error));
                if (index == 3) warm_created = alMetalResourceAllocationStats().textures_created;
                REQUIRE(f.renderer->renderTargetPoolStats().cached_bytes <= 2048ull * 1024ull * 1024ull);
            }
            REQUIRE(f.scaler->stats().creation_successes == 1);
            REQUIRE(f.scaler->stats().reset_calls == 1);
            REQUIRE(f.scaler->stats().submitted_calls == 12);
            const auto pool = f.renderer->renderTargetPoolStats();
            const auto allocations = alMetalResourceAllocationStats();
            std::cout << "Temporal post12 pool acquisitions=" << pool.acquisitions << ", reuses=" << pool.reuses
                      << ", deferrals=" << pool.dependency_deferrals << ", cached_bytes=" << pool.cached_bytes
                      << ", textures_created=" << allocations.textures_created - created
                      << ", created_after_warm4=" << allocations.textures_created - warm_created << '\n';
        }
    }
    const auto after = alMetalResourceAllocationStats();
    REQUIRE(after.texture_bytes_live == before.texture_bytes_live);
    REQUIRE(after.buffer_bytes_live == before.buffer_bytes_live);
}
