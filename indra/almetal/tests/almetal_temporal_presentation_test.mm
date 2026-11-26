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
    std::unique_ptr<ALMetalRenderTarget> source, depth, motion, reconstructed, resolved, opaque_source_depth;
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
        motion = renderer->prepareTemporalMotionTarget(IW, IH, &error); REQUIRE(motion);
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
    std::unique_ptr<ALMetalRenderTarget> makeOpaqueDepthSource(float clear_depth)
    {
        // Preserve the authored depth32 input used by historical DoF/scaler
        // checks. The new API consumes the actual main G-buffer format.
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = IW; descriptor.height = IH; descriptor.color_attachment_count = 0;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        auto source = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(source);
        auto command = device->makeCommandBuffer("Temporal ownership authored G-buffer depth", &error); REQUIRE(command);
        ALMetalRenderPassConfig config; config.clear_depth = clear_depth; config.depth_store = ALMetalStoreAction::store;
        auto pass = source->makeRenderPass(config, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Temporal ownership clear G-buffer", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return source;
    }
    void prepareOpaqueDepthSource()
    {
        REQUIRE_FALSE(opaque_source_depth);
        opaque_source_depth = makeOpaqueDepthSource(0.f);
    }
    std::unique_ptr<ALMetalRenderTarget> opaqueDepth()
    {
        auto result = renderer->prepareTemporalOpaqueDepth(IW, IH, &error);
        INFO(error); REQUIRE(result); return result;
    }
    void captureOpaqueMotion(ALMetalRenderTarget& snapshot,
        const ALMetalRenderTarget* depth_source = nullptr, float depth_motion_scale = 0.f)
    {
        const auto* source = depth_source ? depth_source : opaque_source_depth.get();
        REQUIRE(source);
        REQUIRE(renderer->captureTemporalOpaqueDepth(*source->depthStencilTexture(), snapshot, &error)); INFO(error);
        // The ownership fixture has an authored clear world rather than scene
        // geometry. Consume the snapshot through the production motion API,
        // reusing its already prepared pack just as the viewer does.
        ALMetalWorldGBufferScene opaque;
        opaque.width = IW; opaque.height = IH; opaque.reverse_z = true;
        ALMetalWorldTemporalMotionScene scene;
        scene.opaque_scene = &opaque; scene.depth = source->depthStencilTexture();
        scene.opaque_depth = snapshot.colorTexture(0);
        scene.width = IW; scene.height = IH; scene.reverse_z = true; scene.reset = false;
        scene.clip_to_previous_clip[0] = scene.clip_to_previous_clip[5] =
            scene.clip_to_previous_clip[10] = scene.clip_to_previous_clip[15] = 1.f;
        scene.clip_to_previous_clip[8] = depth_motion_scale;
        motion = renderer->renderTemporalMotion(scene, &error, std::move(motion));
        INFO(error); REQUIRE(motion);
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
    ALMetalWorldTemporalPostConfig postConfig()
    {
        ALMetalWorldTemporalPostConfig config;
        config.width = OW; config.height = OH;
        config.presentation_width = OW; config.presentation_height = OH;
        return config;
    }
    ALMetalWorldTemporalPostTargets postTargets(const ALMetalWorldTemporalPostConfig& config)
    {
        auto result = renderer->prepareTemporalPostTargets(config, &error);
        INFO(error); REQUIRE(result); return std::move(*result);
    }
    std::unique_ptr<ALMetalRenderTarget> display(const ALMetalTexture* adapted = nullptr,
        std::unique_ptr<ALMetalRenderTarget> prepared = {})
    {
        ALMetalWorldDisplayPostScene scene; scene.source = resolved.get(); scene.hdr = true;
        scene.adapted_exposure = .25f; scene.adapted_exposure_texture = adapted;
        auto result = renderer->renderDisplayPost(scene, &error, std::move(prepared)); REQUIRE(result); INFO(error); return result;
    }
    std::unique_ptr<ALMetalRenderTarget> present(const ALMetalRenderTarget& input, bool invalid = false,
        std::unique_ptr<ALMetalRenderTarget> prepared = {})
    {
        ALMetalWorldPresentationScene scene; scene.source = &input; scene.output_width = OW; scene.output_height = OH;
        if (invalid) scene.lens_distortion_amount = -1.f;
        return renderer->renderPresentationPost(scene, &error, std::move(prepared));
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
        constexpr std::array<float, 2> depths{.625f, .375f};
        constexpr float depth_motion_scale = .125f;
        // Author immutable inputs and acquire retained per-world outputs before
        // the gate. No setup wait may release or drain the two pending worlds.
        std::array<std::unique_ptr<ALMetalRenderTarget>, 2> depth_sources, motions;
        auto authored_motion = std::move(f.motion);
        for (unsigned index = 0; index < 2; ++index)
        {
            depth_sources[index] = f.makeOpaqueDepthSource(depths[index]);
            motions[index] = f.renderer->prepareTemporalMotionTarget(IW, IH, &f.error);
            INFO(f.error); REQUIRE(motions[index]);
        }
        PendingGate gate(f);
        std::array<std::shared_ptr<const ALMetalCommandCompletion>, 2> completion;
        std::array<std::shared_ptr<Fixture::Settlement>, 2> settlement;
        std::array<std::unique_ptr<ALMetalRenderTarget>, 2> outputs;
        std::array<std::uint64_t, 2> opaque_identities{}, opaque_generations{};
        const auto submitted = ALMetalDevice::committedCommandBufferCount();
        for (unsigned index = 0; index < 2; ++index)
        {
            f.frame_data->beginFrame(index + 1);
            auto snapshot = f.opaqueDepth();
            opaque_identities[index] = snapshot->colorTexture(0)->resourceView().identity();
            opaque_generations[index] = snapshot->colorTexture(0)->resourceView().generation();
            f.motion = std::move(motions[index]);
            REQUIRE(f.renderer->beginMainFrameBatch(index + 1, "Temporal pending required world", &f.error));
            auto targets = f.postTargets(f.postConfig());
            f.captureOpaqueMotion(*snapshot, depth_sources[index].get(), depth_motion_scale);
            motions[index] = std::move(f.motion);
            // Preserve this existing image/history check's immutable zero-motion
            // input. The extra retained pack separately tests depth-dependent
            // snapshot ordering, without inventing motion for the static HDR image.
            f.motion = std::move(authored_motion);
            auto ticket = f.temporal(*f.source);
            authored_motion = std::move(f.motion);
            auto display = f.display(nullptr, std::move(targets.display));
            outputs[index] = f.present(*display, false, std::move(targets.presentation)); REQUIRE(outputs[index]);
            settlement[index] = f.observe(ticket); completion[index] = f.renderer->commandBatchCompletion();
            REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settlement[index]->history_submitted);
            REQUIRE(settlement[index]->published == 1);
            REQUIRE_FALSE(completion[index]->isComplete());
        }
        REQUIRE(gate.state->load() == 0);
        REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted + 2);
        REQUIRE(f.scaler->stats().submitted_calls == 2); REQUIRE(f.scaler->stats().reset_calls == 1);
        REQUIRE(outputs[0]->colorTexture(0)->nativeHandle() != outputs[1]->colorTexture(0)->nativeHandle());
        // ResourcePlan deliberately permits a new generation after commit:
        // ordered GPU dependencies protect prior uses even before completion.
        // A reused native allocation must advance generation; data below proves
        // both pending worlds consumed their own snapshot before its overwrite.
        if (opaque_identities[0] == opaque_identities[1])
            REQUIRE(opaque_generations[1] > opaque_generations[0]);
        std::cout << "Temporal pending snapshots: identities=" << opaque_identities[0] << ',' << opaque_identities[1]
                  << ", generations=" << opaque_generations[0] << ',' << opaque_generations[1] << '\n';
        f.scaler.reset(); gate.release();
        for (const auto& token : completion) REQUIRE(token->waitUntilCompleted(&f.error));
        for (unsigned index = 0; index < 2; ++index)
        {
            std::vector<float> depth(IW * IH);
            std::vector<__fp16> motion(IW * IH * 2);
            REQUIRE(motions[index]->colorTexture(2)->readback(*f.device,0,0,{},depth.data(),IW * 4,&f.error));
            REQUIRE(motions[index]->colorTexture(0)->readback(*f.device,0,0,{},motion.data(),IW * 4,&f.error));
            // Final depth is preserved independently; the nonzero x displacement
            // is reconstructed specifically from the copied opaque depth.
            const float expected_motion = depths[index] * depth_motion_scale * (.5f * IW);
            for (unsigned pixel = 0; pixel < IW * IH; ++pixel)
            {
                REQUIRE(std::abs(depth[pixel] - depths[index]) < 1.e-6f);
                REQUIRE(std::abs(float(motion[2 * pixel]) - expected_motion) < .002f);
                REQUIRE(std::abs(float(motion[2 * pixel + 1])) < .002f);
            }
        }
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

TEST_CASE("Temporal repeated post transactions reach steady-state pooled target reuse", "[metal][temporal-presentation]")
{
    const auto before = alMetalResourceAllocationStats();
    {
        @autoreleasepool
        {
            Fixture f;
            auto config = f.postConfig();
            const char* configuration = "display, presentation and per-world opaque depth";
            bool capture_opaque_depth = true;
            bool reacquire_visibility = false;
            SECTION("same configuration as the historical 69494 twelve-world fixture")
            { configuration = "display and presentation (historical no snapshot)"; capture_opaque_depth = false; }
            SECTION("per-world pristine depth is reused after completion") {}
            SECTION("per-world private visibility depth is pooled after completion")
            { configuration = "display, presentation, opaque snapshot and visibility input pack"; reacquire_visibility = true; }
            SECTION("bloom pyramid without halation")
            {
                configuration = "bloom and per-world opaque depth"; config.bloom_mip_count = 4; config.bloom_resolution_scale = 2;
            }
            SECTION("halation cross filter and reference capture")
            {
                configuration = "halation cross-filter reference and per-world opaque depth";
                config.bloom_mip_count = 4; config.bloom_resolution_scale = 2;
                config.bloom_halation = true; config.cross_filter = true; config.reference_capture = true;
            }
            if (capture_opaque_depth) f.prepareOpaqueDepthSource();
            if (reacquire_visibility) f.motion.reset();
            const auto created = alMetalResourceAllocationStats().textures_created;
            std::uint64_t warm_created = created, warm_reuses = 0, warm_deferrals = 0;
            std::size_t targets_per_world = 0;
            const void* warm_opaque_backing = nullptr;
            const void* warm_visibility_backing = nullptr;
            for (unsigned index = 0; index < 12; ++index)
            {
                f.frame_data->beginFrame(index + 1);
                if (reacquire_visibility)
                    f.motion = f.renderer->prepareTemporalMotionTarget(IW, IH, &f.error);
                REQUIRE(f.motion);
                const auto* visibility_depth = f.motion->depthStencilTexture(); REQUIRE(visibility_depth);
                REQUIRE(visibility_depth->descriptor().storage == ALMetalStorageMode::memoryless);
                const auto* visibility_backing = visibility_depth->nativeHandle();
                auto snapshot = capture_opaque_depth ? f.opaqueDepth() : nullptr;
                const void* opaque_backing = snapshot ? snapshot->colorTexture(0)->nativeHandle() : nullptr;
                REQUIRE(f.renderer->beginMainFrameBatch(index + 1, "Temporal steady-state pool sequence", &f.error));
                auto targets = f.postTargets(config);
                targets_per_world = 2 + (snapshot ? 1 : 0) + (reacquire_visibility ? 1 : 0) + targets.bloom.levels.size() +
                    (config.cross_filter ? 3 : 0) + (config.reference_capture ? 1 : 0);
                const auto display_backing = targets.display->colorTexture(0)->nativeHandle();
                const auto presentation_backing = targets.presentation->colorTexture(0)->nativeHandle();
                if (snapshot) f.captureOpaqueMotion(*snapshot);
                auto ticket = f.temporal(*f.source);
                std::unique_ptr<ALMetalRenderTarget> bloom, cross_filter, reference;
                if (config.bloom_mip_count)
                {
                    ALMetalWorldBloomScene scene;
                    scene.source = f.resolved.get(); scene.mip_count = config.bloom_mip_count;
                    scene.resolution_scale = config.bloom_resolution_scale;
                    scene.halation = config.bloom_halation; scene.cross_filter = config.cross_filter;
                    bloom = f.renderer->renderBloom(scene, &f.error, &cross_filter, std::move(targets.bloom));
                    INFO(f.error); REQUIRE(bloom);
                    REQUIRE(static_cast<bool>(cross_filter) == config.cross_filter);
                }
                ALMetalWorldDisplayPostScene scene;
                scene.source = f.resolved.get(); scene.hdr = true;
                scene.bloom_texture = bloom ? bloom->colorTexture(0) : nullptr;
                scene.bloom_strength = bloom ? .325f : 0.f;
                scene.cross_filter_texture = cross_filter ? cross_filter->colorTexture(0) : nullptr;
                scene.cross_filter_strength = cross_filter ? .1f : 0.f;
                auto display = f.renderer->renderDisplayPost(scene, &f.error, std::move(targets.display));
                INFO(f.error); REQUIRE(display);
                REQUIRE(display->colorTexture(0)->nativeHandle() == display_backing);
                if (config.reference_capture)
                {
                    const auto reference_backing = targets.reference->colorTexture(0)->nativeHandle();
                    reference = f.renderer->capturePresentationReference(*display, &f.error, std::move(targets.reference));
                    INFO(f.error); REQUIRE(reference);
                    REQUIRE(reference->colorTexture(0)->nativeHandle() == reference_backing);
                }
                auto final = f.present(*display, false, std::move(targets.presentation)); REQUIRE(final);
                REQUIRE(final->colorTexture(0)->nativeHandle() == presentation_backing);
                auto settled = f.observe(ticket);
                auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
                REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settled->history_submitted);
                // Settle only in this offscreen fixture to distinguish genuine
                // concurrency growth from the historical per-world churn.
                REQUIRE(completion->waitUntilCompleted(&f.error));
                const auto pool = f.renderer->renderTargetPoolStats();
                if (index == 3)
                {
                    warm_created = alMetalResourceAllocationStats().textures_created;
                    warm_reuses = pool.reuses; warm_deferrals = pool.dependency_deferrals;
                    warm_opaque_backing = opaque_backing;
                    warm_visibility_backing = visibility_backing;
                }
                if (index > 3)
                {
                    REQUIRE(alMetalResourceAllocationStats().textures_created == warm_created);
                    REQUIRE(pool.dependency_deferrals == warm_deferrals);
                    REQUIRE(opaque_backing == warm_opaque_backing); // Specifically reuse the added R32 owner.
                    REQUIRE(visibility_backing == warm_visibility_backing);
                }
                REQUIRE(pool.cached_bytes <= 2048ull * 1024ull * 1024ull);
                if (reacquire_visibility) f.motion.reset();
            }
            REQUIRE(f.scaler->stats().creation_successes == 1);
            REQUIRE(f.scaler->stats().reset_calls == 1);
            REQUIRE(f.scaler->stats().submitted_calls == 12);
            const auto pool = f.renderer->renderTargetPoolStats();
            const auto allocations = alMetalResourceAllocationStats();
            REQUIRE(pool.reuses - warm_reuses >= 8 * targets_per_world);
            REQUIRE(allocations.textures_created == warm_created);
            std::cout << "Temporal post12 config=" << configuration << ", pool acquisitions=" << pool.acquisitions
                      << ", reuses=" << pool.reuses << ", deferrals=" << pool.dependency_deferrals
                      << ", cached_bytes=" << pool.cached_bytes << ", textures_created=" << allocations.textures_created - created
                      << ", created_after_warm4=" << allocations.textures_created - warm_created << '\n';
        }
    }
    const auto after = alMetalResourceAllocationStats();
    REQUIRE(after.texture_bytes_live == before.texture_bytes_live);
    REQUIRE(after.buffer_bytes_live == before.buffer_bytes_live);
}

TEST_CASE("Temporal prepared post resources validate descriptors and survive abandonment", "[metal][temporal-presentation]")
{
    const auto before = alMetalResourceAllocationStats();
    {
        @autoreleasepool
        {
            Fixture f;
            f.prepareOpaqueDepthSource();
            auto config = f.postConfig();
            config.width = 129; config.height = 97;
            config.presentation_width = 133; config.presentation_height = 101;
            config.bloom_mip_count = 4; config.bloom_resolution_scale = 1;
            config.bloom_halation = true; config.cross_filter = true; config.reference_capture = true;
            auto abandoned_snapshot = f.opaqueDepth();
            const auto abandoned_backing = abandoned_snapshot->colorTexture(0)->nativeHandle();
            const auto submitted_before_copy = ALMetalDevice::committedCommandBufferCount();
            REQUIRE(f.renderer->beginMainFrameBatch(1, "Temporal prepared resources abandoned before encoding", &f.error));
            {
                auto targets = f.postTargets(config);
                auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
                f.captureOpaqueMotion(*abandoned_snapshot);
                REQUIRE(f.scaler->stats().encoded_calls == 0);
                REQUIRE(targets.display->descriptor().width == 129);
                REQUIRE(targets.display->descriptor().height == 97);
                REQUIRE(targets.presentation->descriptor().width == 133);
                REQUIRE(targets.presentation->descriptor().height == 101);
                REQUIRE(targets.reference->descriptor().colors[0].format == ALMetalPixelFormat::rgb10a2_unorm);
                REQUIRE(targets.bloom.levels.size() == 4);
                REQUIRE(targets.bloom.levels[0]->descriptor().width == 96);
                REQUIRE(targets.bloom.levels[0]->descriptor().height == 72);
                REQUIRE(targets.bloom.levels[3]->descriptor().width == 12);
                REQUIRE(targets.bloom.cross_filter[0]->descriptor().width == 48);
                f.renderer->cancelCommandBatch();
                REQUIRE(completion->isComplete());
                REQUIRE(completion->status() == ALMetalCommandStatus::unknown);
                REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted_before_copy);
            }
            abandoned_snapshot.reset();
            const auto created = alMetalResourceAllocationStats().textures_created;
            auto reused_snapshot = f.opaqueDepth();
            REQUIRE(reused_snapshot->colorTexture(0)->nativeHandle() == abandoned_backing);
            REQUIRE(f.renderer->beginMainFrameBatch(1, "Temporal reuses unsubmitted prepared resources", &f.error));
            {
                auto targets = f.postTargets(config);
                f.captureOpaqueMotion(*reused_snapshot);
                REQUIRE(alMetalResourceAllocationStats().textures_created == created);
                f.renderer->cancelCommandBatch();
            }
            reused_snapshot.reset();
            const auto submitted = ALMetalDevice::committedCommandBufferCount();
            auto rejected_snapshot = f.opaqueDepth();
            const auto rejected_backing = rejected_snapshot->colorTexture(0)->nativeHandle();
            REQUIRE(f.renderer->beginMainFrameBatch(1, "Temporal supplied target descriptor rejection", &f.error));
            auto targets = f.postTargets(f.postConfig());
            f.captureOpaqueMotion(*rejected_snapshot);
            auto ticket = f.temporal(*f.source); auto settlement = f.observe(ticket);
            auto rejected_completion = f.renderer->commandBatchCompletion(); REQUIRE(rejected_completion);
            const auto before_rejection = alMetalResourceAllocationStats().textures_created;
            ALMetalWorldDisplayPostScene display; display.source = f.resolved.get(); display.hdr = true;
            // A BGRA presentation owner cannot silently become a fresh display
            // allocation after native encoding has made the command opaque.
            REQUIRE_FALSE(f.renderer->renderDisplayPost(display, &f.error, std::move(targets.presentation)));
            REQUIRE(f.error.find("Pre-acquired Temporal display-post") != std::string::npos);
            REQUIRE(alMetalResourceAllocationStats().textures_created == before_rejection);
            REQUIRE_FALSE(f.renderer->commitCommandBatch(&f.error));
            REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted);
            REQUIRE(settlement->rejected == 1); REQUIRE(settlement->published == 0);
            REQUIRE(rejected_completion->isComplete());
            REQUIRE(rejected_completion->status() == ALMetalCommandStatus::unknown);
            rejected_snapshot.reset();
            auto recovered_snapshot = f.opaqueDepth();
            REQUIRE(recovered_snapshot->colorTexture(0)->nativeHandle() == rejected_backing);
            REQUIRE(alMetalResourceAllocationStats().textures_created == before_rejection);
        }
    }
    const auto after = alMetalResourceAllocationStats();
    REQUIRE(after.texture_bytes_live == before.texture_bytes_live);
    REQUIRE(after.buffer_bytes_live == before.buffer_bytes_live);
}

TEST_CASE("Temporal glow alpha returns from jittered input to unjittered output coordinates", "[metal][temporal-presentation]")
{
    @autoreleasepool
    {
        Fixture f;
        const std::array<std::array<float, 2>, 3> phases{{{0.f,0.f},{.375f,.375f},{-.375f,-.375f}}};
        for (unsigned index = 0; index < phases.size(); ++index)
        {
            const auto jitter = phases[index];
            std::vector<__fp16> values(IW * IH * 4);
            for (unsigned y = 0; y < IH; ++y) for (unsigned x = 0; x < IW; ++x)
            {
                auto* p = values.data() + (y * IW + x) * 4;
                p[0] = .25f; p[1] = .5f; p[2] = .75f;
                // Independent analytical field sampled with the same spatial
                // offset as rasterized geometry. Its output must be centered,
                // not shifted like the original lower-resolution attachment.
                p[3] = .15f + .25f * (float(x) + .5f - jitter[0]) / float(IW) +
                                .5f * (float(y) + .5f - jitter[1]) / float(IH);
            }
            auto upload = f.device->makeCommandBuffer("Authored jittered glow-alpha field", &f.error); REQUIRE(upload);
            REQUIRE(const_cast<ALMetalTexture*>(f.source->colorTexture(0))->upload(*upload,0,0,{},values.data(),IW*8,&f.error));
            REQUIRE(upload->commitAndWait(&f.error));
            f.frame_data->beginFrame(index + 1);
            REQUIRE(f.renderer->beginMainFrameBatch(index + 1, "Temporal RGB and centered glow-alpha", &f.error));
            ALMetalTemporalScalerFrame frame; frame.jitter_x = jitter[0]; frame.jitter_y = jitter[1];
            ALMetalTemporalScaler::Ticket ticket = 0;
            REQUIRE(f.renderer->encodeTemporalUpscale(*f.scaler,f.desc,*f.source->colorTexture(0),*f.motion,
                *f.reconstructed,frame,ticket,&f.error)); INFO(f.error);
            // The null/default path is the existing zero-jitter contract.
            REQUIRE(f.renderer->resolveTemporalColor(*f.source,*f.reconstructed,*f.resolved,&f.error,
                index ? jitter.data() : nullptr)); INFO(f.error);
            auto settlement = f.observe(ticket);
            REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settlement->history_submitted);
            const auto pixels = f.readHDR(*f.resolved);
            const auto raw = f.readHDR(*f.reconstructed);
            for (unsigned y = 4; y + 4 < OH; ++y) for (unsigned x = 4; x + 4 < OW; ++x)
            {
                const auto& p = pixels[y * OW + x];
                for (unsigned c = 0; c < 3; ++c)
                {
                    REQUIRE(std::abs(p[c] - raw[y * OW + x][c]) < .002f);
                    REQUIRE(std::abs(p[c] - Pixel{.25f,.5f,.75f,1.f}[c]) < .008f);
                }
                const float expected = .15f + .25f * (float(x) + .5f) / float(OW) +
                                               .5f * (float(y) + .5f) / float(OH);
                REQUIRE(std::abs(p[3] - expected) < .0015f);
            }
        }
    }
}

TEST_CASE("Temporal display post requires an output-size dirt plate and executes its bloom response", "[metal][temporal-presentation]")
{
    @autoreleasepool
    {
        Fixture f;
        ALMetalWorldLensDirtScene plate; plate.width=IW; plate.height=IH;
        auto stale=f.renderer->renderLensDirt(plate,&f.error); REQUIRE(stale); INFO(f.error);
        plate.width=OW; plate.height=OH;
        auto current=f.renderer->renderLensDirt(plate,&f.error); REQUIRE(current); INFO(f.error);
        auto bloom=f.target(1,1,ALMetalPixelFormat::rgba16_float);
        auto upload=f.device->makeCommandBuffer("Temporal auxiliary bloom input",&f.error); REQUIRE(upload);
        const std::array<__fp16,4> bloom_value{.25f,.5f,.75f,0.f};
        REQUIRE(const_cast<ALMetalTexture*>(bloom->colorTexture(0))->upload(*upload,0,0,{},bloom_value.data(),8,&f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        const auto submitted=ALMetalDevice::committedCommandBufferCount();
        REQUIRE(f.renderer->beginMainFrameBatch(1,"Temporal stale auxiliary extent",&f.error));
        auto ticket=f.temporal(*f.source);
        auto settlement=f.observe(ticket);
        ALMetalWorldDisplayPostScene display;
        display.source=f.resolved.get(); display.hdr=true; display.adapted_exposure=.25f;
        display.bloom_texture=bloom->colorTexture(0); display.bloom_strength=.5f;
        display.lens_dirt_texture=stale->colorTexture(0); display.lens_dirt_strength=1.f;
        REQUIRE_FALSE(f.renderer->renderDisplayPost(display,&f.error));
        REQUIRE(f.error.find("dirt is incompatible") != std::string::npos);
        REQUIRE_FALSE(f.renderer->commitCommandBatch(&f.error));
        REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted);
        REQUIRE(settlement->rejected == 1); REQUIRE(settlement->published == 0);
        REQUIRE_FALSE(f.scaler->stats().history_valid);
        f.frame_data->beginFrame(2);
        REQUIRE(f.renderer->beginMainFrameBatch(1,"Temporal correctly sized auxiliary extent",&f.error));
        ticket=f.temporal(*f.source); settlement=f.observe(ticket);
        display.lens_dirt_texture=current->colorTexture(0);
        auto dirty=f.renderer->renderDisplayPost(display,&f.error); REQUIRE(dirty); INFO(f.error);
        display.lens_dirt_texture=nullptr; display.lens_dirt_strength=0.f;
        auto clean=f.renderer->renderDisplayPost(display,&f.error); REQUIRE(clean); INFO(f.error);
        REQUIRE(dirty->descriptor().width == OW); REQUIRE(dirty->descriptor().height == OH);
        auto dirty_final=f.present(*dirty); REQUIRE(dirty_final); INFO(f.error);
        auto clean_final=f.present(*clean); REQUIRE(clean_final); INFO(f.error);
        REQUIRE(f.renderer->commitCommandBatch(&f.error)); REQUIRE(settlement->history_submitted);
        const auto dirty_pixels=f.readBytes(*dirty_final), clean_pixels=f.readBytes(*clean_final);
        unsigned changed=0;
        for (std::size_t i=0;i<dirty_pixels.size();++i)
            changed += dirty_pixels[i][0] != clean_pixels[i][0] || dirty_pixels[i][1] != clean_pixels[i][1] ||
                       dirty_pixels[i][2] != clean_pixels[i][2];
        REQUIRE(changed > 0);
        REQUIRE(f.scaler->stats().submitted_calls == 1);
    }
}
