/**
 * @file almetal_temporal_orchestration_test.mm
 * @brief Viewer display-eligibility rejection abandons the actual Temporal transaction.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include "../almetaltemporalworldhistory.h"
#include "../almetalretainedbackground.h"
#include "../../newview/pipeline_main_post_transaction_native.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
constexpr unsigned IW = 32, IH = 24, OW = 64, OH = 48;
using Bytes = std::array<std::uint8_t, 4>;
void identity(float* matrix)
{
    std::fill_n(matrix, 16, 0.f);
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.f;
}
ALMetalMainPostDisplayEligibility eligible()
{
    return {true, true, true, true, true, true, true, true, true, true};
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalTemporalScaler> scaler;
    ALMetalTemporalWorldHistory history;
    ALMetalTemporalScalerDescriptor descriptor{IW, IH, OW, OH};
    std::shared_ptr<ALMetalWorldFrameData> frame_data;
    std::unique_ptr<ALMetalRenderTarget> source, motion, raw, resolved;
    std::shared_ptr<const ALMetalRenderTarget> presentation, world_depth;
    ALMetalRetainedBackground retained;
    ALMetalRetainedBackground::Context context;
    ALMetalTemporalScaler::Ticket ticket = 0;
    ALMetalWorldSimpleBatch pose;
    ALMetalWorldGBufferScene opaque;
    std::uint64_t world = 0;
    bool post_active = false, opaque_active = false, current_presentation = false;
    bool temporal_submitted = false, successful_mode_published = false;
    unsigned display_calls = 0, canceled = 0, settled_false = 0, settled_true = 0;
    unsigned successful_history_commits = 0;

    Fixture()
    {
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled); REQUIRE(device); INFO(error);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error); REQUIRE(renderer); INFO(error);
        scaler = ALMetalTemporalScaler::create(*device, &error); REQUIRE(scaler); INFO(error);
        ALMetalTemporalScalerRequirements requirements;
        REQUIRE(scaler->prepare(descriptor, requirements, &error)); INFO(error);
        frame_data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frame_data);
        renderer->setFrameData(frame_data);
        ALMetalRenderTargetDescriptor color;
        color.width = IW; color.height = IH; color.colors[0].format = ALMetalPixelFormat::rgba16_float;
        source = ALMetalRenderTarget::create(*device, color, &error); REQUIRE(source);
        ALMetalRenderTargetDescriptor dd;
        dd.width = IW; dd.height = IH; dd.color_attachment_count = 0;
        dd.depth_stencil_format = ALMetalPixelFormat::depth32_float; dd.depth_stencil_shader_read = true;
        world_depth = ALMetalRenderTarget::create(*device, dd, &error); REQUIRE(world_depth);
        context = {device.get(), this, 1, OW, OH, true};
        ALMetalRenderTargetDescriptor pack;
        pack.width = IW; pack.height = IH; pack.color_attachment_count = 3;
        pack.colors[0].format = ALMetalPixelFormat::rg16_float;
        pack.colors[1].format = ALMetalPixelFormat::r8_unorm;
        pack.colors[2].format = ALMetalPixelFormat::r32_float;
        motion = ALMetalRenderTarget::create(*device, pack, &error); REQUIRE(motion);
        raw = renderer->prepareTemporalTarget(OW, OH, true, &error); REQUIRE(raw);
        resolved = renderer->prepareTemporalTarget(OW, OH, false, &error); REQUIRE(resolved);
        auto upload = device->makeCommandBuffer("Temporal orchestration authored inputs", &error); REQUIRE(upload);
        std::vector<__fp16> pixels(IW * IH * 4), vectors(IW * IH * 2, 0.f);
        std::vector<std::uint8_t> reactive(IW * IH, 0);
        std::vector<float> depth(IW * IH, .5f);
        for (unsigned i = 0; i < IW * IH; ++i)
        {
            pixels[4 * i] = .2f; pixels[4 * i + 1] = .4f;
            pixels[4 * i + 2] = .8f; pixels[4 * i + 3] = .25f;
        }
        REQUIRE(const_cast<ALMetalTexture*>(source->colorTexture(0))->upload(*upload,0,0,{},pixels.data(),IW * 8,&error));
        REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(0))->upload(*upload,0,0,{},vectors.data(),IW * 4,&error));
        REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(1))->upload(*upload,0,0,{},reactive.data(),IW,&error));
        REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(2))->upload(*upload,0,0,{},depth.data(),IW * 4,&error));
        ALMetalRenderPassConfig clear;
        clear.clear_depth = .5; clear.depth_store = ALMetalStoreAction::store;
        auto pass = world_depth->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*upload, *pass, "Temporal retained-world reversed depth", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error));
        REQUIRE(upload->commitAndWait(&error));
        pose.temporal_instance_id = 71; pose.temporal_generation = 1;
        pose.vertex_count = pose.index_count = 3;
        identity(pose.modelview_projection); identity(pose.texture_matrix);
        opaque.batches = &pose; opaque.batch_count = 1;
    }
    ~Fixture()
    {
        if (renderer) renderer->cancelCommandBatch();
        if (device) device->waitUntilIdle();
    }
    void begin(std::uint64_t id)
    {
        world = id; ticket = 0; current_presentation = false; temporal_submitted = false;
        presentation.reset();
        successful_mode_published = false;
        ALMetalTemporalWorldHistory::FrameInput input;
        input.world_id = id; input.input_width = IW; input.input_height = IH;
        input.output_width = OW; input.output_height = OH; input.mode = 5;
        identity(input.camera_mvp);
        REQUIRE(history.beginFrame(input, &error));
        REQUIRE(history.stage(opaque, nullptr, &error));
        frame_data->beginFrame(id);
        REQUIRE(renderer->beginMainFrameBatch(id, "Temporal viewer orchestration", &error));
        post_active = opaque_active = true;
        ALMetalTemporalScalerFrame frame;
        const auto jitter = history.jitterPixels();
        frame.jitter_x = jitter[0]; frame.jitter_y = jitter[1]; frame.reset = history.resetRequired();
        REQUIRE(renderer->encodeTemporalUpscale(*scaler, descriptor, *source->colorTexture(0),
            *motion, *raw, frame, ticket, &error)); INFO(error); REQUIRE(ticket);
        REQUIRE(renderer->resolveTemporalColor(*source, *raw, *resolved, &error, jitter.data()));
        REQUIRE(renderer->observeCommandBatchSettlement([this, id](bool committed)
        {
            if (committed)
            {
                ++settled_true;
                // Match the owning viewer's guard: native commit by itself
                // never advances an unfinished world or publishes its mode.
                if (current_presentation && ticket && scaler->submitted(ticket, &error) && history.commit(id, &error))
                { ticket = 0; temporal_submitted = true; ++successful_history_commits; }
                else abandon();
            }
            else { ++settled_false; abandon(); }
        }, &error));
    }
    void abandon()
    {
        if (ticket) scaler->abandon(ticket);
        ticket = 0; history.abandon();
    }
    bool finalize(bool temporal = true)
    {
        return alMetalCancelIncompleteTemporalPost(temporal, current_presentation,
            post_active, opaque_active, [this]
            {
                ++canceled;
                renderer->cancelCommandBatch();
                abandon(); history.invalidate(); scaler->invalidate();
            });
    }
    void displayConsumer()
    {
        ++display_calls;
        ALMetalWorldDisplayPostScene display;
        display.source = resolved.get(); display.hdr = true;
        auto displayed = renderer->renderDisplayPost(display, &error); REQUIRE(displayed); INFO(error);
        ALMetalWorldPresentationScene final;
        final.source = displayed.get(); final.output_width = OW; final.output_height = OH;
        auto presented = renderer->renderPresentationPost(final, &error); REQUIRE(presented); INFO(error);
        presentation = std::move(presented);
        current_presentation = true;
    }
    void displayIfEligible(const ALMetalMainPostDisplayEligibility& inputs)
    {
        // This is the production viewer decision, reached after Temporal
        // encode but BEFORE renderDisplayPost consumes the required inputs.
        if (inputs.ready()) displayConsumer();
    }
    void genericCleanup()
    {
        finalize();
        if (post_active)
        {
            if (opaque_active) REQUIRE(renderer->commitCommandBatch(&error));
            else renderer->cancelCommandBatch();
            post_active = opaque_active = false;
        }
        if (current_presentation && temporal_submitted)
        {
            successful_mode_published = true;
            REQUIRE(retained.publish(context, world, presentation, world_depth, &error)); INFO(error);
            // The viewer releases per-frame products between displays. The
            // existing retained-world authority keeps the last valid image.
            presentation.reset();
        }
    }
    std::vector<Bytes> pixels()
    {
        const auto record = retained.find(context); REQUIRE(record); REQUIRE(record->colorTexture());
        std::vector<Bytes> data(OW * OH);
        REQUIRE(record->colorTexture()->readback(*device,0,0,{},data.data(),OW * 4,&error));
        return data;
    }
};
}

TEST_CASE("Viewer display eligibility rejects missing resources before its consumer", "[temporal-orchestration]")
{
    auto ready = eligible();
    REQUIRE(ready.ready());
    for (auto member : {&ALMetalMainPostDisplayEligibility::presentation, &ALMetalMainPostDisplayEligibility::sky,
        &ALMetalMainPostDisplayEligibility::exposure, &ALMetalMainPostDisplayEligibility::depth_of_field,
        &ALMetalMainPostDisplayEligibility::bloom, &ALMetalMainPostDisplayEligibility::cross_filter,
        &ALMetalMainPostDisplayEligibility::lens_flare, &ALMetalMainPostDisplayEligibility::lens_dirt,
        &ALMetalMainPostDisplayEligibility::legacy_glow_noise, &ALMetalMainPostDisplayEligibility::color_grade})
    {
        auto missing = ready; missing.*member = false;
        CHECK_FALSE(missing.ready());
    }
}

TEST_CASE("Viewer rejects an encoded Temporal world before display without submitting it and recovers", "[metal][temporal-orchestration]")
{
    const auto before = alMetalResourceAllocationStats();
    {
        @autoreleasepool
        {
            Fixture f;
            f.begin(1); f.displayIfEligible(eligible()); f.genericCleanup();
            REQUIRE(f.successful_mode_published); REQUIRE(f.temporal_submitted);
            const auto previous = f.pixels();
            const auto previous_record = f.retained.find(f.context); REQUIRE(previous_record);
            REQUIRE_FALSE(f.presentation);
            REQUIRE(previous_record->source_frame == 1);
            const auto previous_backing = previous_record->colorTexture()->nativeHandle();
            const auto previous_generation = previous_record->colorTexture()->resourceView().generation();
            const auto submitted = ALMetalDevice::committedCommandBufferCount();
            const auto main_submitted = f.renderer->mainFrameStats().committed;
            const auto accepted_worlds = f.history.stats().submitted_worlds;
            const auto accepted_phase = f.history.stats().jitter_phase;
            const auto accepted_resets = f.history.stats().resets;
            const auto scaler_submitted = f.scaler->stats().submitted_calls;
            const auto reset_calls = f.scaler->stats().reset_calls;
            const auto display_calls = f.display_calls;
            auto required = eligible();
            SECTION("required lens dirt is unavailable") { required.lens_dirt = false; }
            SECTION("required color grade is unavailable") { required.color_grade = false; }
            f.pose.modelview_projection[12] = .5f;
            f.begin(2);
            REQUIRE_FALSE(f.history.resetRequired());
            const auto pending_phase = f.history.stats().jitter_phase;
            REQUIRE(pending_phase == 1);
            REQUIRE(f.history.previousOpaque().size() == 1);
            REQUIRE(f.history.previousOpaque()[0].modelview_projection[12] == 0.f);
            const auto encoded = f.scaler->stats().encoded_calls;
            REQUIRE(encoded == 2);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
            f.displayIfEligible(required);
            REQUIRE(f.display_calls == display_calls);
            REQUIRE_FALSE(f.current_presentation);
            REQUIRE(f.finalize());
            REQUIRE_FALSE(f.finalize()); // Repeated owner cleanup is inert.
            f.genericCleanup(); // The actual generic partial-commit decision cannot submit it.
            REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted);
            REQUIRE(f.renderer->mainFrameStats().committed == main_submitted);
            REQUIRE_FALSE(f.renderer->commandBatchActive());
            REQUIRE(f.settled_false == 1); REQUIRE(f.settled_true == 1); REQUIRE(f.canceled == 1);
            REQUIRE(f.successful_history_commits == 1); REQUIRE_FALSE(f.successful_mode_published);
            REQUIRE_FALSE(f.temporal_submitted); REQUIRE_FALSE(f.history.pending());
            REQUIRE(f.history.stats().submitted_worlds == accepted_worlds);
            REQUIRE(f.history.stats().jitter_phase == accepted_phase); // Reset discarded the pending phase; it never advances.
            REQUIRE(f.history.stats().resets == accepted_resets);
            REQUIRE(f.history.stats().abandoned_worlds == 1);
            REQUIRE(f.scaler->stats().submitted_calls == scaler_submitted);
            REQUIRE(f.scaler->stats().abandoned_calls == 1);
            REQUIRE_FALSE(f.scaler->stats().history_valid);
            REQUIRE(completion->isComplete()); REQUIRE(completion->status() == ALMetalCommandStatus::unknown);
            const auto retained_after_rejection = f.retained.find(f.context);
            REQUIRE(retained_after_rejection == previous_record);
            REQUIRE(retained_after_rejection->source_frame == 1);
            REQUIRE(retained_after_rejection->colorTexture()->nativeHandle() == previous_backing);
            REQUIRE(retained_after_rejection->colorTexture()->resourceView().generation() == previous_generation);
            REQUIRE_FALSE(f.presentation);
            REQUIRE(f.pixels() == previous);
            f.begin(3);
            REQUIRE(f.history.resetRequired()); REQUIRE(f.history.stats().jitter_phase == 0);
            REQUIRE_FALSE(f.history.previousOpaque()[0].valid);
            f.displayIfEligible(eligible()); f.genericCleanup();
            REQUIRE(f.temporal_submitted); REQUIRE(f.successful_mode_published);
            REQUIRE(f.successful_history_commits == 2); REQUIRE(f.settled_true == 2);
            REQUIRE(f.history.stats().submitted_worlds == accepted_worlds + 1);
            REQUIRE(f.history.stats().resets == accepted_resets + 1);
            REQUIRE(f.scaler->stats().submitted_calls == scaler_submitted + 1);
            REQUIRE(f.scaler->stats().reset_calls == reset_calls + 1);
            REQUIRE(f.retained.find(f.context)->source_frame == 3);
            REQUIRE(f.retained.find(f.context) != previous_record);
            REQUIRE_FALSE(f.presentation);
            REQUIRE(f.pixels() == previous);
        }
    }
    const auto after = alMetalResourceAllocationStats();
    REQUIRE(after.textures_created - after.textures_destroyed == before.textures_created - before.textures_destroyed);
    REQUIRE(after.texture_bytes_live == before.texture_bytes_live);
    REQUIRE(after.buffer_bytes_live == before.buffer_bytes_live);
}

TEST_CASE("Incomplete Native and Spatial batches preserve their partial-commit decision", "[metal][temporal-orchestration]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error); REQUIRE(renderer);
        for (unsigned mode : {0u, 1u, 2u})
        {
            INFO(mode);
            REQUIRE(renderer->beginMainFrameBatch(mode + 1, "Non-Temporal partial batch", &error));
            const auto before = ALMetalDevice::committedCommandBufferCount();
            bool post_active = true, opaque_active = true;
            unsigned cancellations = 0;
            REQUIRE_FALSE(alMetalCancelIncompleteTemporalPost(false, false, post_active, opaque_active,
                [&] { ++cancellations; renderer->cancelCommandBatch(); }));
            REQUIRE(post_active); REQUIRE(opaque_active); REQUIRE(cancellations == 0);
            REQUIRE(renderer->commitCommandBatch(&error));
            REQUIRE(ALMetalDevice::committedCommandBufferCount() == before + 1);
        }
        device->waitUntilIdle();
    }
}
