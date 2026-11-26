/** Genuine pre-transparency capture used by the native Aurora component.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetaltemporalworldhistory.h"
#include "../../newview/pipeline_main_post_transaction_native.h"

namespace
{
using Pixel = std::array<float, 4>;
std::vector<Pixel> auroraPixels(const ALMetalTexture& texture, const ALMetalDevice& device, std::string& error)
{
    REQUIRE(texture.descriptor().format == ALMetalPixelFormat::rgba16_float);
    std::vector<std::array<__fp16, 4>> half(std::size_t(texture.width()) * texture.height());
    REQUIRE(texture.readback(device, 0, 0, {}, half.data(), texture.width() * 8, &error));
    INFO(error);
    std::vector<Pixel> out(half.size());
    for (std::size_t i = 0; i < out.size(); ++i)
        std::copy(half[i].begin(), half[i].end(), out[i].begin());
    return out;
}
std::vector<std::uint32_t> auroraPresentationBytes(const ALMetalTexture& texture, const ALMetalDevice& device, std::string& error)
{
    REQUIRE(texture.descriptor().format == ALMetalPixelFormat::bgra8_unorm);
    std::vector<std::uint32_t> bytes(std::size_t(texture.width()) * texture.height());
    REQUIRE(texture.readback(device, 0, 0, {}, bytes.data(), texture.width() * 4, &error));
    return bytes;
}

TEST_CASE("Aurora borrowed main transaction cancels before display and resets on recovery", "[metal][aurora-inputs]")
{
    Fixture            f;
    constexpr unsigned output_side = 48;
    auto               aurora      = ALMetalAuroraReconstructor::create(*f.device, &f.error);
    INFO(f.error);
    REQUIRE(aurora);
    aurora->setFrameData(f.data);
    ALMetalAuroraDescriptor descriptor{ side, side, output_side, output_side };
    REQUIRE(aurora->prepare(descriptor, &f.error));
    auto source = f.renderer->prepareTemporalTarget(side, side, false, &f.error);
    REQUIRE(source);
    auto opaque = f.renderer->prepareTemporalTarget(side, side, false, &f.error);
    REQUIRE(opaque);
    auto motion = f.renderer->prepareTemporalMotionTarget(side, side, &f.error);
    REQUIRE(motion);
    auto output = f.renderer->prepareTemporalTarget(output_side, output_side, false, &f.error);
    REQUIRE(output);
    auto upload = f.device->makeCommandBuffer("Aurora orchestration fixed HDR and motion inputs", &f.error);
    REQUIRE(upload);
    const std::array<__fp16, 4>        hdr{ .1f, .4f, .8f, .25f };
    std::vector<std::array<__fp16, 4>> colors(side * side, hdr);
    std::vector<std::array<__fp16, 2>> vectors(side * side);
    std::vector<std::uint8_t>          rejection(side * side, 0);
    std::vector<float>                 depth(side * side, .05f);
    for (const auto* target : { source.get(), opaque.get() })
        REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(*upload, 0, 0, {}, colors.data(), side * 8, &f.error));
    REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(0))->upload(*upload, 0, 0, {}, vectors.data(), side * 4, &f.error));
    REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(1))->upload(*upload, 0, 0, {}, rejection.data(), side, &f.error));
    REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(2))->upload(*upload, 0, 0, {}, depth.data(), side * 4, &f.error));
    REQUIRE(upload->commitAndWait(&f.error));
    ALMetalTemporalWorldHistory history;
    auto                        pose = f.batch(0);
    pose.temporal_instance_id        = 917;
    pose.temporal_generation         = 3;
    auto scene                       = f.scene();
    scene.batches                    = &pose;
    scene.batch_count                = 1;
    std::shared_ptr<const ALMetalRenderTarget> last_presentation;
    std::unique_ptr<ALMetalRenderTarget>       latest_bloom;
    unsigned                                   settled_true = 0, settled_false = 0, advanced = 0, canceled = 0;
    const auto                                 world = [&](std::uint64_t id, bool display_eligible)
    {
        ALMetalTemporalWorldHistory::FrameInput input;
        input.world_id    = id;
        input.input_width = input.input_height = side;
        input.output_width = input.output_height = output_side;
        input.mode                               = 8;
        input.jitter_phase_count                 = 18;
        identity(input.camera_mvp);
        REQUIRE(history.beginFrame(input, &f.error));
        REQUIRE(history.stage(scene, nullptr, &f.error));
        f.data->beginFrame(id);
        ALMetalWorldTemporalPostConfig config;
        config.width = config.height = output_side;
        config.presentation_width = config.presentation_height = output_side;
        config.bloom_mip_count                                 = 3;
        auto targets                                           = f.renderer->prepareTemporalPostTargets(config, &f.error);
        INFO(f.error);
        REQUIRE(targets);
        REQUIRE(f.renderer->beginMainFrameBatch(id, "Aurora main transaction fixture", &f.error));
        ALMetalAuroraFrame frame;
        const auto         jitter                               = history.jitterPixels();
        frame.jitter_x                                          = jitter[0];
        frame.jitter_y                                          = jitter[1];
        frame.jitter_phase_count                                = 18;
        frame.reset                                             = history.resetRequired();
        frame.projection                                        = { .5f, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, 0, .1f, 0 };
        ALMetalAuroraReconstructor::Ticket ticket               = 0;
        bool                               current_presentation = false, post = true, opaque_active = true;
        REQUIRE(f.renderer->encodeAuroraUpscale(*aurora, descriptor, *source->colorTexture(0), *opaque->colorTexture(0), *motion, *output,
                                                frame, ticket, &f.error));
        INFO(f.error);
        REQUIRE(ticket);
        auto completion = f.renderer->commandBatchCompletion();
        REQUIRE(completion);
        REQUIRE(f.renderer->observeCommandBatchSettlement(
            [&](bool committed)
            {
                if (committed)
                {
                    ++settled_true;
                    REQUIRE(current_presentation);
                    REQUIRE(aurora->submitted(ticket, &f.error));
                    REQUIRE(history.commit(id, &f.error));
                    ++advanced;
                }
                else
                {
                    ++settled_false;
                    aurora->abandon(ticket);
                    history.abandon();
                    history.invalidate();
                    aurora->invalidate();
                }
                ticket = 0;
            },
            &f.error));
        ALMetalMainPostDisplayEligibility eligibility{ true, true, true, true, true, true, true, display_eligible, true, true };
        if (eligibility.ready())
        {
            ALMetalWorldBloomScene bloom;
            bloom.source    = output.get();
            bloom.mip_count = 3;
            bloom.threshold = 100.f;
            bloom.knee      = 0.f; // RGB below threshold: alpha alone supplies legacy glow.
            latest_bloom    = f.renderer->renderBloom(bloom, &f.error, nullptr, std::move(targets->bloom));
            INFO(f.error);
            REQUIRE(latest_bloom);
            ALMetalWorldDisplayPostScene display;
            display.source         = output.get();
            display.hdr            = true;
            display.bloom_texture  = latest_bloom->colorTexture(0);
            display.bloom_strength = .325f;
            auto displayed         = f.renderer->renderDisplayPost(display, &f.error, std::move(targets->display));
            INFO(f.error);
            REQUIRE(displayed);
            ALMetalWorldPresentationScene final;
            final.source       = displayed.get();
            final.output_width = final.output_height = output_side;
            auto presented = f.renderer->renderPresentationPost(final, &f.error, std::move(targets->presentation));
            INFO(f.error);
            REQUIRE(presented);
            last_presentation    = std::move(presented);
            current_presentation = true;
        }
        auto cancel = [&]
        {
            ++canceled;
            f.renderer->cancelCommandBatch();
            if (ticket)
            {
                aurora->abandon(ticket);
                ticket = 0;
                history.abandon();
                history.invalidate();
                aurora->invalidate();
            }
        };
        const bool rejected = alMetalCancelIncompleteTemporalPost(true, current_presentation, post, opaque_active, cancel);
        REQUIRE(rejected == !display_eligible);
        REQUIRE_FALSE(alMetalCancelIncompleteTemporalPost(true, current_presentation, post, opaque_active, cancel));
        if (post)
            REQUIRE(f.renderer->commitCommandBatch(&f.error));
        if (!display_eligible)
        {
            REQUIRE(completion->status() == ALMetalCommandStatus::unknown);
            REQUIRE(completion->isComplete());
        }
        return frame.reset;
    };
    REQUIRE(world(1, true));
    REQUIRE(last_presentation);
    const auto previous = last_presentation;
    REQUIRE(latest_bloom);
    const auto* bloom_texture = latest_bloom->colorTexture(0);
    REQUIRE(bloom_texture->descriptor().format == ALMetalPixelFormat::rg11b10_float);
    std::vector<std::uint32_t> bloom_pixels(bloom_texture->width() * bloom_texture->height());
    REQUIRE(bloom_texture->readback(*f.device, 0, 0, {}, bloom_pixels.data(), bloom_texture->width() * 4, &f.error));
    REQUIRE(std::any_of(bloom_pixels.begin(), bloom_pixels.end(), [](auto p) { return p != 0; }));
    const auto before    = auroraPresentationBytes(*previous->colorTexture(0), *f.device, f.error);
    const auto submitted = ALMetalDevice::committedCommandBufferCount(), accepted = history.stats().submitted_worlds;
    const auto phase = history.stats().jitter_phase, parity = aurora->stats().parity;
    pose.modelview_projection[12] = .25f;
    REQUIRE_FALSE(world(2, false));
    REQUIRE(ALMetalDevice::committedCommandBufferCount() == submitted);
    REQUIRE(history.stats().submitted_worlds == accepted);
    REQUIRE(history.stats().jitter_phase == phase);
    REQUIRE(aurora->stats().parity == parity);
    REQUIRE_FALSE(aurora->stats().history_valid);
    REQUIRE(last_presentation == previous);
    REQUIRE(auroraPresentationBytes(*previous->colorTexture(0), *f.device, f.error) == before);
    REQUIRE(settled_true == 1);
    REQUIRE(settled_false == 1);
    REQUIRE(canceled == 1);
    REQUIRE(advanced == 1);
    REQUIRE(world(3, true));
    REQUIRE(last_presentation != previous);
    REQUIRE(history.stats().submitted_worlds == 2);
    REQUIRE(settled_true == 2);
    REQUIRE(settled_false == 1);
    REQUIRE(advanced == 2);
    REQUIRE(aurora->stats().submitted_calls == 2);
    REQUIRE(aurora->stats().abandoned_calls == 1);
}
} // namespace

TEST_CASE("Aurora opaque input is captured before actual blended world color", "[metal][aurora-inputs]")
{
    Fixture f;
    auto    s     = f.scene();
    s.hdr         = true;
    s.reverse_z   = true;
    s.batch_count = 0;
    auto gbuffer  = f.renderer->render(s, &f.error);
    INFO(f.error);
    REQUIRE(gbuffer);
    auto hdr = f.renderer->prepareTemporalTarget(side, side, false, &f.error);
    REQUIRE(hdr);
    auto opaque = f.renderer->prepareTemporalTarget(side, side, false, &f.error);
    REQUIRE(opaque);
    const std::array<__fp16, 4>        blue{ .05f, .1f, .3f, .2f };
    std::vector<std::array<__fp16, 4>> background(side * side, blue);
    auto                               upload = f.device->makeCommandBuffer("Aurora pre-transparency input fixture", &f.error);
    REQUIRE(upload);
    REQUIRE(const_cast<ALMetalTexture*>(hdr->colorTexture(0))->upload(*upload, 0, 0, {}, background.data(), side * 8, &f.error));
    REQUIRE(upload->commitAndWait(&f.error));

    Fixture::Geometry geometry{};
    const float       positions[4][2]{ { -.7f, -.7f }, { .7f, -.7f }, { .7f, .7f }, { -.7f, .7f } };
    for (unsigned i = 0; i < 4; ++i)
    {
        std::copy_n(positions[i], 2, geometry.positions[i]);
        geometry.positions[i][2] = .3f;
        geometry.positions[i][3] = 1.f;
        geometry.normals[i][2]   = 1.f;
        std::fill_n(geometry.colors[i], 4, std::uint8_t(255));
    }
    const std::array<std::uint16_t, 6> indices{ 0, 1, 2, 0, 2, 3 };
    auto                               leaf = f.batch(0);
    leaf.vertices                           = f.buffer(&geometry, sizeof(geometry));
    leaf.indices                            = f.buffer(indices.data(), sizeof(indices));
    leaf.index_count                        = 6;
    leaf.vertex_count                       = 4;
    const std::array<__fp16, 4> green{ .1f, .8f, .1f, .75f };
    leaf.diffuse_textures[0]     = f.texture(ALMetalPixelFormat::rgba16_float, 1, green.data(), 8);
    leaf.post_water_alpha        = true;
    leaf.pre_water_fullbright    = true;
    leaf.pre_water_minimum_alpha = .004f;

    ALMetalWorldAlphaEnvironment environment;
    environment.water_plane[2] = 1.f;
    environment.water_plane[3] = -100.f;
    environment.water_fog[0] = environment.water_fog[2] = 1.f;
    environment.glow_max_y[0]                           = 1.f;
    environment.glow_max_y[2]                           = -1.f;
    environment.glow_max_y[3]                           = 1.f;
    environment.light_direction[2]                      = 1.f;
    environment.atmospheric[1] = environment.atmospheric[2] = environment.atmospheric[3] = 1.f;
    environment.atmos_sunlit[3] = environment.atmos_sunlit_linear[3] = 1.f;
    environment.environment[0]                                       = 1.f;
    for (unsigned i = 0; i < 3; ++i)
        environment.eye_to_sky[i][i] = 1.f;
    ALMetalWorldLegacyAlphaLighting lighting;
    lighting.sun_direction[2] = 1.f;
    lighting.parameters[0]    = 128.f;
    lighting.parameters[1]    = .004f;
    lighting.parameters[2]    = 1.f;
    ALMetalTextureDescriptor cube;
    cube.type   = ALMetalTextureType::texture_cube;
    cube.format = ALMetalPixelFormat::rgba8_unorm;
    cube.usage  = ALMetalTextureUsage::shader_read;
    auto black  = ALMetalTexture::create(*f.device, cube, &f.error);
    REQUIRE(black);
    auto cube_upload = f.device->makeCommandBuffer("Aurora neutral forward environment", &f.error);
    REQUIRE(cube_upload);
    const std::uint8_t zero[4]{ 0, 0, 0, 255 };
    for (unsigned face = 0; face < 6; ++face)
        REQUIRE(black->upload(*cube_upload, 0, face, {}, zero, 4, &f.error));
    REQUIRE(cube_upload->commitAndWait(&f.error));

    auto descriptor                      = hdr->descriptor();
    descriptor.colors[0].texture         = hdr->colorTexture(0);
    descriptor.depth_stencil_format      = gbuffer->descriptor().depth_stencil_format;
    descriptor.depth_stencil_shader_read = true;
    descriptor.depth_stencil_texture     = gbuffer->depthStencilTexture();
    auto continuation                    = ALMetalRenderTarget::create(*f.device, descriptor, &f.error);
    REQUIRE(continuation);
    ALMetalWorldOpaquePostDeferredScene forward;
    forward.width = forward.height = side;
    forward.hdr                    = true;
    forward.reverse_z              = true;
    forward.batches                = &leaf;
    forward.batch_count            = 1;
    forward.gbuffer                = gbuffer.get();
    forward.composed_lighting      = continuation.get();
    forward.alpha_environment      = &environment;
    forward.alpha_lighting         = &lighting;
    forward.legacy_environment     = black.get();
    forward.reconstruction_opaque  = opaque.get();
    REQUIRE(f.renderer->beginCommandBatch("Aurora actual color producer and pre-transparency capture", &f.error));
    auto final = f.renderer->renderOpaquePostDeferred(forward, std::move(continuation), &f.error);
    INFO(f.error);
    REQUIRE(final);
    REQUIRE(f.renderer->commitCommandBatch(&f.error));
    INFO(f.error);
    const auto before = auroraPixels(*opaque->colorTexture(0), *f.device, f.error),
               after  = auroraPixels(*final->colorTexture(0), *f.device, f.error);
    const auto center = (side / 2) * side + side / 2, corner = 1u;
    for (unsigned c = 0; c < 4; ++c)
    {
        REQUIRE(before[center][c] == float(blue[c]));
        REQUIRE(before[corner][c] == float(blue[c]));
    }
    REQUIRE(after[center][1] > before[center][1] + .1f);
    REQUIRE(after[center][0] != before[center][0]);
    for (unsigned c = 0; c < 3; ++c)
        REQUIRE(after[corner][c] == before[corner][c]);

    SECTION("Opaque-only path still initializes the genuine capture")
    {
        forward.batch_count               = 0;
        forward.composed_lighting         = final.get();
        auto next_descriptor              = final->descriptor();
        next_descriptor.colors[0].texture = final->colorTexture(0);
        auto borrowed                     = ALMetalRenderTarget::create(*f.device, next_descriptor, &f.error);
        REQUIRE(borrowed);
        forward.composed_lighting = borrowed.get();
        auto result               = f.renderer->renderOpaquePostDeferred(forward, std::move(borrowed), &f.error);
        INFO(f.error);
        REQUIRE(result);
        const auto captured = auroraPixels(*opaque->colorTexture(0), *f.device, f.error);
        REQUIRE(captured == after);
    }
}
