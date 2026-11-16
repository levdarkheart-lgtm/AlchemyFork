/**
 * @file almetal_main_lighting_renderpass_test.mm
 * @brief Actual main lighting chain parity, transaction ownership and costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_sky_modernization_fixture.h"
#include "../almetalstagetiming.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace
{
using Status = ALMetalWorldLightingChainStatus;
using Clock = std::chrono::steady_clock;

bool hasRGBIncrease(const std::vector<std::array<std::uint16_t, 4>>& after,
                    const std::vector<std::array<std::uint16_t, 4>>& before)
{
    if (after.size() != before.size()) return false;
    for (std::size_t pixel = 0; pixel < after.size(); ++pixel)
        for (unsigned channel = 0; channel < 3; ++channel)
            if (after[pixel][channel] != before[pixel][channel] &&
                sky_test::half(after[pixel][channel]) > sky_test::half(before[pixel][channel])) return true;
    return false;
}

struct Inputs
{
    sky_test::Fixture& f;
    std::unique_ptr<ALMetalRenderTarget> gbuffer;
    const ALMetalTexture* light_map = nullptr;
    const ALMetalTexture* cookie = nullptr;
    const ALMetalTexture* environment = nullptr;
    ALMetalWorldDirectionalShadows shadows;
    std::shared_ptr<ALMetalVisibilityResultBatch> queries;
    std::array<std::uint64_t, 2> query_values{};
    ALMetalWorldBaseLightingScene base;
    ALMetalWorldModernSkyScene sky;
    bool all_fullscreen_points = false;
    std::vector<ALMetalWorldLocalLight> point_recipe;
    std::vector<ALMetalWorldProjectorLight> projector_recipe;

    Inputs(sky_test::Fixture& fixture, unsigned width = 32, unsigned height = 32,
           bool reverse = false, bool hdr = true, bool normal_hdr = true,
           bool pbr = false, bool with_queries = false) : f(fixture)
    {
        // The fixture's real geometry/texture helpers author the inputs only.
        // Half the view is geometry; half is clear-depth sky.
        struct alignas(16) Streams
        {
            float position[4][4]{{-1,-1,0,1},{0,-1,0,1},{-1,1,0,1},{0,1,0,1}};
            float normal[4][4]{};
            float uv[4][2]{};
            std::uint8_t color[4][4]{};
            float tangent[4][4]{};
        } data;
        for (unsigned i = 0; i < 4; ++i)
        {
            data.position[i][2] = reverse ? .75f : .25f;
            data.normal[i][2] = 1.f;
            data.uv[i][0] = data.uv[i][1] = .5f;
            data.tangent[i][0] = data.tangent[i][3] = 1.f;
            std::fill_n(data.color[i], 4, std::uint8_t{255});
        }
        const std::array<std::uint16_t, 6> indices{0,1,2,2,1,3};
        ALMetalWorldSimpleBatch batch;
        batch.vertices = f.buffer(data); batch.indices = f.buffer(indices);
        batch.vertex_count = 4; batch.index_count = 6; batch.double_sided = true;
        batch.normal_offset = offsetof(Streams, normal);
        batch.texcoord_offset = offsetof(Streams, uv);
        batch.color_offset = offsetof(Streams, color);
        batch.tangent_offset = offsetof(Streams, tangent);
        batch.texture_count = 1; batch.diffuse_textures[0] = f.flat({.25f,.375f,.5f,1.f});
        sky_test::identity(batch.modelview_projection); sky_test::identity(batch.modelview);
        for (unsigned axis = 0; axis < 3; ++axis) batch.normal_matrix[axis][axis] = 1.f;
        if (pbr)
        {
            batch.pbr_slot_count = 1;
            auto& material = batch.pbrSlots()[0];
            material.normal_texture = f.flat({.5f,.5f,1.f,1.f});
            material.orm_texture = f.flat({1.f,.4f,.25f,1.f});
            material.emissive_texture = f.flat({.03f,.01f,.02f,1.f});
            material.roughness_factor = .7f; material.metallic_factor = .4f;
        }
        std::array<ALMetalWorldVisibilityQuery, 2> visibility{};
        for (auto& query : visibility)
        {
            sky_test::identity(query.modelview_projection);
            query.center[0] = .5f; query.center[3] = 1.f;
            query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .2f;
            query.fan = 7;
        }
        visibility[0].count_samples = 1; visibility[1].center[0] = 4.f;
        ALMetalWorldGBufferScene scene;
        scene.width = width; scene.height = height;
        scene.reverse_z = reverse; scene.hdr = hdr; scene.normal_hdr = normal_hdr;
        scene.batches = &batch; scene.batch_count = 1;
        if (with_queries)
        { scene.visibility_queries = visibility.data(); scene.visibility_query_count = visibility.size(); }
        gbuffer = f.renderer->render(scene, &f.error, with_queries ? &queries : nullptr);
        INFO(f.error); REQUIRE(gbuffer);
        if (with_queries)
        {
            REQUIRE(queries); REQUIRE(queries->resultCount() == query_values.size());
            REQUIRE(queries->wait(0, query_values[0], &f.error));
            REQUIRE(queries->result(1, query_values[1]));
            CHECK(query_values[0] > 0); CHECK(query_values[1] == 0);
        }
        f.stampStencil(*gbuffer);

        ALMetalTextureDescriptor descriptor;
        descriptor.width = width; descriptor.height = height;
        descriptor.format = ALMetalPixelFormat::rg8_unorm;
        auto map = ALMetalTexture::create(*f.device, descriptor, &f.error); REQUIRE(map);
        std::vector<std::uint8_t> white(width * height * 2, 255);
        auto upload = f.device->makeCommandBuffer("Main lighting fixed light map", &f.error); REQUIRE(upload);
        REQUIRE(map->upload(*upload, 0, 0, {}, white.data(), width * 2, &f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        light_map = map.get(); f.maps.push_back(std::move(map));
        auto cube = f.renderer->createNeutralModernSkyCube(&f.error); REQUIRE(cube);
        environment = cube.get(); f.maps.push_back(std::move(cube));
        cookie = f.flat({.8f,.6f,.4f,1.f});

        base.gbuffer = gbuffer.get(); base.light_map = light_map;
        base.legacy_environment = environment; base.hdr = hdr;
        base.constants.environment_config[0] = 1;
        sky_test::identity(base.constants.inverse_projection);
        base.constants.inverse_projection[14] = -1.f;
        base.constants.light_direction[2] = 1.f;
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            base.constants.atmos_sunlit[channel] = .7f;
            base.constants.atmos_sunlit_linear[channel] = .7f;
            base.constants.atmos_amblit[channel] = .15f;
        }
        base.constants.lighting[0] = base.constants.lighting[1] = 1.f;
        base.constants.surface[1] = normal_hdr ? 1.f : 0.f;
        base.constants.config[0] = reverse;
        base.carry_depth = base.borrow_depth = base.depth_stencil_shader_read = true;
        base.diagnostic_view.view_kind = 1;
        sky = f.scene(128, reverse); sky.width = width; sky.height = height;
        sky.constants.config[1] = hdr;
        sky.hdri_texture = f.flat({.125f,.25f,.75f,1.f});
        populate(2, 2);
    }

    void populate(unsigned points, unsigned projectors)
    {
        point_recipe.assign(points, {}); projector_recipe.assign(projectors, {});
        const auto width = gbuffer->descriptor().width, height = gbuffer->descriptor().height;
        for (unsigned i = 0; i < points; ++i)
        {
            auto& light = point_recipe[i];
            light.position_radius[0] = -.25f + .02f * i;
            light.position_radius[2] = .5f; light.position_radius[3] = 3.f;
            light.color_falloff[0] = .08f; light.color_falloff[1] = .06f; light.color_falloff[2] = .04f;
            if (i < points / 2)
            { light.screen_rect[2] = width / 2; light.screen_rect[3] = height; }
        }
        for (unsigned i = 0; i < projectors; ++i)
        {
            auto& light = projector_recipe[i]; light.cookie_texture = cookie;
            light.position_radius[0] = -.25f; light.position_radius[2] = .5f; light.position_radius[3] = 3.f;
            light.color_falloff[0] = .12f; light.color_falloff[1] = .08f; light.color_falloff[2] = .06f;
            light.projector_matrix[0] = light.projector_matrix[5] = .25f;
            light.projector_matrix[12] = light.projector_matrix[13] = .5f;
            light.projector_matrix[14] = light.projector_matrix[15] = 1.f;
            light.projector_normal[2] = -1.f; light.projector_normal[3] = 2.f;
            light.projector_origin[2] = .5f; light.projector_origin[3] = 2.f;
            light.sampling[0] = 1.f;
            if (i % 2 == 0) { light.screen_rect[2] = width / 2; light.screen_rect[3] = height; }
        }
    }

    ALMetalWorldLocalLightingScene local(const std::vector<ALMetalWorldLocalLight>& lights) const
    {
        ALMetalWorldLocalLightingScene result;
        result.gbuffer = gbuffer.get(); result.lights = lights.data(); result.light_count = lights.size();
        result.bounded_light_count = all_fullscreen_points ? 0 : lights.size() / 2;
        result.additive_pass_count = point_recipe.size() + projector_recipe.size();
        std::copy_n(base.constants.inverse_projection, 16, result.inverse_projection);
        result.reverse_z = base.constants.config[0]; result.hdr = base.hdr;
        result.classic_mode = base.constants.config[1] != 0;
        return result;
    }

    ALMetalWorldProjectorLightingScene projector(const std::vector<ALMetalWorldProjectorLight>& lights) const
    {
        ALMetalWorldProjectorLightingScene result;
        result.gbuffer = gbuffer.get(); result.lights = lights.data(); result.light_count = lights.size();
        result.additive_pass_count = point_recipe.size() + projector_recipe.size();
        std::copy_n(base.constants.inverse_projection, 16, result.inverse_projection);
        result.reverse_z = base.constants.config[0]; result.hdr = base.hdr;
        result.normal_hdr = gbuffer->descriptor().colors[2].format == ALMetalPixelFormat::rgba16_unorm;
        result.classic_mode = base.constants.config[1] != 0;
        if (std::any_of(lights.begin(), lights.end(), [](const auto& light) { return light.shadow_channel >= 0; }))
            result.projector_shadows = &shadows;
        return result;
    }

    void enableShadows()
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_2d_array;
        descriptor.format = ALMetalPixelFormat::r32_float;
        descriptor.width = descriptor.height = 8; descriptor.array_length = 2;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        auto texture = ALMetalTexture::create(*f.device, descriptor, &f.error); REQUIRE(texture);
        auto upload = f.device->makeCommandBuffer("Actual projector shadow input for main chain", &f.error); REQUIRE(upload);
        std::array<float, 64> depths;
        for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x)
            depths[y * 8 + x] = (x + y) % 2 ? .25f : .75f;
        for (unsigned slice = 0; slice < 2; ++slice)
            REQUIRE(texture->upload(*upload, 0, slice, {}, depths.data(), 8 * sizeof(float), &f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        shadows.depth_texture = texture.get(); f.maps.push_back(std::move(texture));
        for (unsigned slice = 0; slice < 2; ++slice)
        {
            auto& matrix = shadows.constants.shadow_matrix[slice];
            matrix[0] = matrix[5] = .25f; matrix[12] = matrix[13] = matrix[14] = .5f; matrix[15] = 1.f;
        }
        shadows.constants.shadow_clip[2] = 1.f; shadows.constants.shadow_clip[3] = 2.f;
        shadows.constants.resolution[0] = shadows.constants.resolution[1] = 8.f;
        shadows.constants.resolution[2] = gbuffer->descriptor().width;
        shadows.constants.resolution[3] = gbuffer->descriptor().height;
        shadows.constants.parameters[2] = shadows.constants.parameters[3] = 32.f;
        shadows.constants.config[0] = 2; shadows.constants.config[3] = 1;
        for (unsigned i = 0; i < projector_recipe.size(); ++i)
        { projector_recipe[i].shadow_channel = i % 2; projector_recipe[i].sampling[1] = .25f; }
    }

    std::vector<std::array<std::uint16_t, 4>> pixels(const ALMetalRenderTarget& target)
    { return f.read<std::array<std::uint16_t, 4>>(*target.colorTexture(0)); }

    void checkQueries() const
    {
        if (!queries) return;
        CHECK(queries->status() == ALMetalVisibilityResultBatch::Status::ready);
        for (unsigned i = 0; i < query_values.size(); ++i)
        { std::uint64_t value = 0; REQUIRE(queries->result(i, value)); CHECK(value == query_values[i]); }
    }
};

// No assertions, readback, diagnostic counter work or synthetic lighting math.
// Both correctness and cost use these exact shipping renderer entrypoints.
std::unique_ptr<ALMetalRenderTarget> encode(ALMetalWorldGBufferRenderer& renderer,
    const Inputs& inputs, bool compound, bool sky, std::string* error, Status* status = nullptr)
{
    // Native list ownership is built within both timed paths. The source
    // recipes are retained fixtures, not live viewer traversal/culling.
    const auto points = inputs.point_recipe;
    const auto projectors = inputs.projector_recipe;
    const auto local = inputs.local(points); const auto projector = inputs.projector(projectors);
    if (compound)
    {
        auto result = renderer.renderMainLightingChain(inputs.base, sky ? &inputs.sky : nullptr,
            local, projector, error);
        if (status) *status = result.status;
        return std::move(result.target);
    }
    auto target = renderer.renderBaseLighting(inputs.base, error);
    if (!target || (sky && !renderer.compositeModernSky(*target,
            *inputs.gbuffer->depthStencilTexture(), inputs.sky, error)) ||
        !renderer.accumulateLocalLights(*target, local, error) ||
        !renderer.accumulateProjectorLights(*target, projector, error)) return nullptr;
    if (status) *status = Status::encoded;
    return target;
}

std::unique_ptr<ALMetalRenderTarget> submit(sky_test::Fixture& f,
    ALMetalWorldGBufferRenderer& renderer, const Inputs& inputs, bool compound, bool sky)
{
    REQUIRE(renderer.beginCommandBatch("Main lighting parity transaction", &f.error));
    Status status = Status::declined;
    auto target = encode(renderer, inputs, compound, sky, &f.error, &status);
    INFO(f.error); REQUIRE(status == Status::encoded); REQUIRE(target);
    REQUIRE(renderer.commitCommandBatch(&f.error)); REQUIRE(f.device->waitUntilIdle(&f.error));
    return target;
}
struct Observed
{
    std::unique_ptr<ALMetalRenderTarget> output;
    std::uint64_t draws = 0, commits = 0, encoders = 0;
};
Observed observe(sky_test::Fixture& f, ALMetalWorldGBufferRenderer& renderer,
    const Inputs& inputs, bool compound, const char* scenario, unsigned pair)
{
    struct RestoreTiming
    {
        bool old = ALMetalDevice::commandTimingEnabled();
        RestoreTiming() { ALMetalDevice::setCommandTimingEnabled(true); }
        ~RestoreTiming() { ALMetalDevice::setCommandTimingEnabled(old); }
    } restore;
    auto window = ALMetalStageTimingWindow::create(*f.device, 16, &f.error); REQUIRE(window);
    const auto draws = ALMetalDevice::encodedDrawCallCount();
    const auto commits = ALMetalDevice::committedCommandBufferCount();
    REQUIRE(renderer.beginCommandBatch("Untimed matching main lighting execution census", &f.error));
    Observed result;
    {
        ALMetalStageTimingScope scope(window.get(), 1, 1, -1);
        result.output = encode(renderer, inputs, compound, true, &f.error);
    }
    INFO(f.error); REQUIRE(result.output); REQUIRE(renderer.commitCommandBatch(&f.error));
    result.draws = ALMetalDevice::encodedDrawCallCount() - draws;
    result.commits = ALMetalDevice::committedCommandBufferCount() - commits;
    result.encoders = window->preparedPassCount();
    REQUIRE(f.device->waitUntilIdle(&f.error));
    REQUIRE(window->seal(&f.error)); std::vector<ALMetalStageTimingResult> results;
    REQUIRE(window->poll(results, &f.error)); CHECK(window->isFinished());
    if (compound)
    {
        std::vector<std::string> fragments;
        for (const auto& stage : results)
            for (const auto& pipeline : stage.pipelines) fragments.push_back(pipeline.fragment);
        for (const char* expected : {"almetal_world_base_light_geometry_fragment", "almetal_world_modern_sky_fragment"})
            CHECK(std::find(fragments.begin(), fragments.end(), expected) != fragments.end());
        if (!inputs.point_recipe.empty()) CHECK(std::find(fragments.begin(), fragments.end(),
            "almetal_world_local_light_fragment") != fragments.end());
        if (!inputs.projector_recipe.empty()) CHECK(std::find(fragments.begin(), fragments.end(),
            "almetal_world_projector_light_fragment") != fragments.end());
    }
    CHECK(result.draws > 0); CHECK(result.commits == 1);
    CHECK(result.encoders == (compound ? 1u : 2u + !inputs.point_recipe.empty() + !inputs.projector_recipe.empty()));
    std::cout << "main_lighting_census scenario=" << scenario << " pair=" << pair
        << " width=" << inputs.gbuffer->descriptor().width << " height=" << inputs.gbuffer->descriptor().height
        << " points=" << inputs.point_recipe.size() << " projectors=" << inputs.projector_recipe.size()
        << " mode=" << compound << " encoders=" << result.encoders << " draws=" << result.draws
        << " commits=" << result.commits << " scope=untimed_actual_matching_API_before_readbacks\n";
    return result;
}

}

TEST_CASE("almetal: one main lighting renderpass preserves serial half output, sky and borrowed stores",
          "[almetal][main-lighting-renderpass]")
{
    @autoreleasepool
    {
        sky_test::Fixture f;
        // A separate same-library owner makes the first compound call a real
        // cold BRDF case even though its serial oracle has already rendered.
        auto serial = ALMetalWorldGBufferRenderer::create(*f.device, 1, f.renderer->shaderLibrary(), &f.error);
        INFO(f.error); REQUIRE(serial);
        for (bool reverse : {false, true}) for (bool hdr : {false, true})
        for (bool normal_hdr : {false, true}) for (bool pbr : {false, true})
        {
            CAPTURE(reverse, hdr, normal_hdr, pbr);
            Inputs inputs(f, 32, 32, reverse, hdr, normal_hdr, pbr, true);
            const auto depth = f.read<std::uint32_t>(*inputs.gbuffer->depthStencilTexture());
            const auto stencil = f.stencil(*inputs.gbuffer->depthStencilTexture());
            std::array<std::vector<std::uint8_t>, 4> stores;
            for (unsigned i = 0; i < stores.size(); ++i) stores[i] = f.bytes(*inputs.gbuffer->colorTexture(i));
            std::array<std::vector<std::array<std::uint16_t, 4>>, 2> base_pixels;
            for (unsigned lights = 0; lights < 4; ++lights) for (bool sky : {false, true})
            {
                CAPTURE(lights, sky);
                inputs.populate((lights & 1u) ? 17 : 0, (lights & 2u) ? 2 : 0);
                // Sky-on cases cross the actual 16-fullscreen-light packet
                // boundary; sky-off cases retain individual bounded draws.
                inputs.all_fullscreen_points = sky;
                auto expected = submit(f, *serial, inputs, false, sky);
                auto actual = submit(f, *f.renderer, inputs, true, sky);
                REQUIRE(actual->descriptor().color_attachment_count == 1);
                CHECK(actual->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
                CHECK(actual->depthStencilTexture() == inputs.gbuffer->depthStencilTexture());
                const auto pixels = inputs.pixels(*actual);
                CHECK(pixels == inputs.pixels(*expected));
                if (lights == 0) base_pixels[sky] = pixels;
                else CHECK(hasRGBIncrease(pixels, base_pixels[sky])); // Alpha alone cannot prove lighting.
                CHECK(std::any_of(pixels.begin(), pixels.end(), [](const auto& pixel)
                    { return (pixel[0] & 0x7fffu) || (pixel[1] & 0x7fffu) || (pixel[2] & 0x7fffu); }));
                if (sky) CHECK(pixels[16 * 32 + 24][2] != 0); // Sky positive control.
                CHECK(f.read<std::uint32_t>(*actual->depthStencilTexture()) == depth);
                CHECK(f.stencil(*actual->depthStencilTexture()) == stencil);
                inputs.checkQueries();
            }
            for (unsigned i = 0; i < stores.size(); ++i) CHECK(f.bytes(*inputs.gbuffer->colorTexture(i)) == stores[i]);
        }
    }
}

TEST_CASE("almetal: main lighting decline preserves prefix and cancellation permits exact retry",
          "[almetal][main-lighting-renderpass]")
{
    @autoreleasepool
    {
        sky_test::Fixture f; Inputs inputs(f, 32, 32, false, true, true, false, true);
        auto local = inputs.local(inputs.point_recipe); auto projector = inputs.projector(inputs.projector_recipe);
        for (unsigned rejected = 0; rejected < 7; ++rejected)
        {
            CAPTURE(rejected);
            REQUIRE(f.renderer->beginCommandBatch("Keep real base prefix across declined chain", &f.error));
            auto prefix = f.renderer->renderBaseLighting(inputs.base, &f.error); REQUIRE(prefix);
            const auto receipt = f.renderer->commandBatchCompletion(); REQUIRE(receipt);
            auto base = inputs.base; auto invalid_local = local; auto invalid_projector = projector;
            std::vector<ALMetalWorldProjectorLight> no_cookie = inputs.projector_recipe;
            switch (rejected)
            {
                case 0: base.carry_depth = false; break;
                case 1: base.borrow_depth = false; break;
                case 2: base.diagnostic_view.view_kind = 2; break;
                case 3: invalid_local.gbuffer = nullptr; break;
                case 4: invalid_projector.hdr = !inputs.base.hdr; break;
                case 5: no_cookie[0].cookie_texture = nullptr; invalid_projector.lights = no_cookie.data(); break;
                case 6: invalid_local.inverse_projection[0] = NAN; break;
            }
            auto result = f.renderer->renderMainLightingChain(base, nullptr, invalid_local, invalid_projector, &f.error);
            CHECK(result.status == Status::declined); CHECK_FALSE(result.target);
            CHECK(f.renderer->commandBatchCompletion() == receipt);
            CHECK_FALSE(receipt->isComplete());
            REQUIRE(f.renderer->commitCommandBatch(&f.error));
            REQUIRE(f.device->waitUntilIdle(&f.error)); CHECK(receipt->isComplete());
            CHECK_FALSE(inputs.pixels(*prefix).empty()); inputs.checkQueries();
        }
        REQUIRE(f.renderer->beginCommandBatch("Cancel fully encoded main chain", &f.error));
        bool settled = true; unsigned settlements = 0;
        REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool ok) { settled = ok; ++settlements; }, &f.error));
        const auto committed = ALMetalDevice::committedCommandBufferCount();
        auto canceled = encode(*f.renderer, inputs, true, true, &f.error); REQUIRE(canceled);
        const auto canceled_receipt = f.renderer->commandBatchCompletion(); REQUIRE(canceled_receipt);
        f.renderer->cancelCommandBatch();
        CHECK_FALSE(settled); CHECK(settlements == 1); CHECK(canceled_receipt->isComplete());
        CHECK(ALMetalDevice::committedCommandBufferCount() == committed); canceled.reset();
        auto expected = submit(f, *f.renderer, inputs, false, true);
        auto retry = submit(f, *f.renderer, inputs, true, true);
        CHECK(inputs.pixels(*retry) == inputs.pixels(*expected)); inputs.checkQueries();
    }
}

TEST_CASE("almetal: shared main encoder preserves projector cookies, shadow slices and classic gain",
          "[almetal][main-lighting-renderpass]")
{
    @autoreleasepool
    {
        sky_test::Fixture f;
        for (bool reverse : {false, true}) for (bool pbr : {false, true})
        {
            CAPTURE(reverse, pbr);
            Inputs inputs(f, 31, 27, reverse, true, true, pbr);
            inputs.enableShadows(); inputs.base.constants.config[1] = 1;
            auto expected = submit(f, *f.renderer, inputs, false, true);
            auto actual = submit(f, *f.renderer, inputs, true, true);
            CHECK(inputs.pixels(*actual) == inputs.pixels(*expected));
            CHECK(f.stencil(*actual->depthStencilTexture()) == f.stencil(*inputs.gbuffer->depthStencilTexture()));
            CHECK(f.read<std::uint32_t>(*actual->depthStencilTexture()) == f.read<std::uint32_t>(*inputs.gbuffer->depthStencilTexture()));
            // Positive control for a light input in this mixed-depth fixture.
            const auto original = inputs.pixels(*expected);
            for (auto& light : inputs.projector_recipe)
                std::fill_n(light.color_falloff, 3, 0.f);
            auto dark = submit(f, *f.renderer, inputs, false, true);
            CHECK(hasRGBIncrease(original, inputs.pixels(*dark)));
            const auto* identity = actual.get();
            const auto* color_identity = actual->colorTexture(0);
            const auto color = inputs.pixels(*actual);
            const auto depth = f.read<std::uint32_t>(*actual->depthStencilTexture());
            const auto stencil = f.stencil(*actual->depthStencilTexture());
            ALMetalWorldOpaquePostDeferredScene continuation;
            continuation.gbuffer = inputs.gbuffer.get(); continuation.composed_lighting = actual.get();
            continuation.width = 31; continuation.height = 27;
            continuation.reverse_z = reverse; continuation.hdr = true;
            auto continued = f.renderer->renderOpaquePostDeferred(
                continuation, std::move(actual), &f.error);
            INFO(f.error); REQUIRE(continued);
            CHECK(continued.get() == identity); CHECK(continued->colorTexture(0) == color_identity);
            CHECK(continued->depthStencilTexture() == inputs.gbuffer->depthStencilTexture());
            CHECK(inputs.pixels(*continued) == color);
            CHECK(f.read<std::uint32_t>(*continued->depthStencilTexture()) == depth);
            CHECK(f.stencil(*continued->depthStencilTexture()) == stencil);
        }
    }
}

TEST_CASE("almetal: cold main chain failure poisons its real prefix and retries BRDF preparation",
          "[almetal][main-lighting-renderpass]")
{
    @autoreleasepool
    {
        sky_test::Fixture f; Inputs inputs(f, 32, 32, false, true, true, true);
        auto serial = ALMetalWorldGBufferRenderer::create(*f.device, 1, f.renderer->shaderLibrary(), &f.error);
        INFO(f.error); REQUIRE(serial);
        REQUIRE(f.renderer->beginCommandBatch("Cold main chain post-base fault", &f.error));
        bool settled = true; unsigned settlements = 0;
        REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool ok) { settled = ok; ++settlements; }, &f.error));
        const auto committed = ALMetalDevice::committedCommandBufferCount();
        f.renderer->failNextMainLightingChainAfterBaseForTesting();
        // Declining before native work must not consume the one-shot fault.
        auto ineligible = inputs.base; ineligible.borrow_depth = false;
        auto declined = f.renderer->renderMainLightingChain(ineligible, nullptr,
            inputs.local(inputs.point_recipe), inputs.projector(inputs.projector_recipe), &f.error);
        CHECK(declined.status == Status::declined); CHECK_FALSE(declined.target);
        auto failed = f.renderer->renderMainLightingChain(inputs.base, &inputs.sky,
            inputs.local(inputs.point_recipe), inputs.projector(inputs.projector_recipe), &f.error);
        CHECK(failed.status == Status::failed); CHECK_FALSE(failed.target);
        CHECK_FALSE(f.renderer->commitCommandBatch(&f.error));
        CHECK_FALSE(settled); CHECK(settlements == 1);
        CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
        auto retry = submit(f, *f.renderer, inputs, true, true);
        // A distinct cold serial owner actually generates its own LUT; it
        // cannot agree merely because a broken retry reused the same bytes.
        auto expected = submit(f, *serial, inputs, false, true);
        CHECK(inputs.pixels(*retry) == inputs.pixels(*expected));
    }
}

TEST_CASE("almetal: main chain records one physical render encoder with all lighting executables",
          "[almetal][main-lighting-renderpass]")
{
    @autoreleasepool
    {
        sky_test::Fixture f; Inputs inputs(f);
        auto warm = submit(f, *f.renderer, inputs, true, true); warm.reset();
        for (unsigned lists = 0; lists < 4; ++lists)
        {
            CAPTURE(lists);
            inputs.populate((lists & 1u) ? 16 : 0, (lists & 2u) ? 4 : 0);
            auto serial = observe(f, *f.renderer, inputs, false, "normal", lists);
            auto chain = observe(f, *f.renderer, inputs, true, "normal", lists);
            CHECK(serial.draws == chain.draws); CHECK(serial.commits == chain.commits);
            CHECK(inputs.pixels(*serial.output) == inputs.pixels(*chain.output));
        }
    }
}

namespace
{
std::uint64_t ownerClock(bool& ok)
{
    timespec now{}; ok = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) == 0;
    return ok ? static_cast<std::uint64_t>(now.tv_sec) * 1000000000ull + now.tv_nsec : 0;
}
struct Cost
{
    bool ok = false, clock_ok = false;
    std::string error;
    double cpu_ms = 0, encode_commit_ms = 0, settled_ms = 0, gpu_ms = 0;
    std::unique_ptr<ALMetalRenderTarget> output;
};
Cost measure(sky_test::Fixture& f, ALMetalWorldGBufferRenderer& renderer,
    ALMetalWorldFrameData& arena, const Inputs& inputs, bool compound, unsigned frame)
{
    Cost result; bool clock_started = false, clock_finished = false;
    const auto wall_started = Clock::now(); const auto cpu_started = ownerClock(clock_started);
    arena.beginFrame(frame);
    if (!renderer.beginCommandBatch("Complete native main lighting cost", &result.error)) return result;
    result.output = encode(renderer, inputs, compound, true, &result.error);
    auto completion = renderer.commandBatchCompletion();
    if (!result.output || !completion || !renderer.commitCommandBatch(&result.error))
    { renderer.cancelCommandBatch(); return result; }
    const auto committed = Clock::now();
    if (!f.device->waitUntilIdle(&result.error) || !completion->isComplete()) return result;
    ALMetalCommandGPUTime gpu;
    if (completion->gpuTime(gpu) != ALMetalCommandTimingStatus::ready)
    { result.error = "Native main cost lacks exact command GPU completion timestamps"; return result; }
    // Retire the output and exact receipt inside timing, after GPU completion.
    result.output.reset();
    completion.reset();
    const auto cpu_finished = ownerClock(clock_finished); const auto wall_finished = Clock::now();
    result.clock_ok = clock_started && clock_finished;
    result.cpu_ms = (cpu_finished - cpu_started) / 1e6;
    result.encode_commit_ms = std::chrono::duration<double, std::milli>(committed - wall_started).count();
    result.settled_ms = std::chrono::duration<double, std::milli>(wall_finished - wall_started).count();
    result.gpu_ms = gpu.elapsed_nanoseconds / 1e6; result.ok = true; return result;
}
double median(std::vector<double> values)
{
    std::sort(values.begin(), values.end()); const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) * .5;
}
bool enabled(const char* name)
{ const char* value = std::getenv(name); return value && value[0] && value[0] != '0'; }
}

TEST_CASE("almetal: complete serial and single-renderpass native main lighting cost frontier",
          "[.][almetal][main-lighting-renderpass-cost]")
{
    if (!enabled("ALCHEMY_MAIN_LIGHTING_RENDERPASS_COST"))
        SKIP("Set ALCHEMY_MAIN_LIGHTING_RENDERPASS_COST=1 for the serialized native frontier");
#ifndef NDEBUG
    SKIP("Main lighting costs require optimized source");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    REQUIRE_FALSE(ALMetalDevice::commandTimingEnabled());
    std::cout << "main_lighting_cost_scope includes=native_list_copy_frame_arena_begin_target_acquisition_base_modernsky_point_projector_encode_commit_exact_GPU_completion_output_retirement"
        " excludes=LL_scene_gather_culling_source_publication_Gbuffer_lightmap_production_asset_upload_renderer_creation"
        " current_serial_same_command_batch=1 native_only_complete_path=1 fullviewer_PPD307_collector=0 GPUwait_included=1"
        " extra_idle_barrier_command=1 exact_command_gpu_ms_excludes_idle_barrier=1"
        " draw_commit_encoder_census=untimed_same_API_matching_cells timed_rows_counter_instrumented=0 validation=off FPS_claim=0\n";
    for (const auto dimensions : {std::array<unsigned, 2>{1920,1080}, {2560,1440}})
    for (unsigned lists = 0; lists < 4; ++lists)
    @autoreleasepool
    {
        sky_test::Fixture f; Inputs inputs(f, dimensions[0], dimensions[1], true, true, true, true);
        inputs.populate((lists & 1u) ? 16 : 0, (lists & 2u) ? 4 : 0);
        std::array<std::unique_ptr<ALMetalWorldGBufferRenderer>, 2> owners;
        std::array<std::shared_ptr<ALMetalWorldFrameData>, 2> arenas;
        for (unsigned mode = 0; mode < owners.size(); ++mode)
        {
            owners[mode] = ALMetalWorldGBufferRenderer::create(*f.device, 1, f.renderer->shaderLibrary(), &f.error);
            INFO(f.error); REQUIRE(owners[mode]);
            arenas[mode] = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(arenas[mode]);
            owners[mode]->setFrameData(arenas[mode]);
        }
        std::array<std::vector<double>, 2> cpu, settled, gpu;
        constexpr unsigned cold = 1, warmups = 2, measured = 8;
        for (unsigned sample = 0; sample < cold + warmups + measured; ++sample)
        {
            std::array<Cost, 2> results;
            for (unsigned order = 0; order < 2; ++order)
            {
                const unsigned mode = order ^ (sample & 1u);
                results[mode] = measure(f, *owners[mode], *arenas[mode], inputs, mode != 0, sample + 1);
                const auto& result = results[mode];
                INFO(result.error); REQUIRE(result.ok); REQUIRE(result.clock_ok);
                if (sample >= cold + warmups)
                { cpu[mode].push_back(result.cpu_ms); settled[mode].push_back(result.settled_ms); gpu[mode].push_back(result.gpu_ms); }
                std::cout << std::setprecision(9) << "main_lighting_cost width=" << dimensions[0]
                    << " height=" << dimensions[1] << " lists=" << lists << " points=" << inputs.point_recipe.size()
                    << " projectors=" << inputs.projector_recipe.size() << " mode=" << mode << " sample=" << sample
                    << " phase=" << (sample < cold ? "cold" : sample < cold + warmups ? "warmup" : "warm")
                    << " owner_cpu_ms=" << result.cpu_ms << " encode_commit_ms=" << result.encode_commit_ms
                    << " settled_wall_ms=" << result.settled_ms << " exact_command_gpu_ms=" << result.gpu_ms << '\n';
            }
        }
        // Pixel validation is a separate untimed pair, with no added readback
        // command, Catch assertion or counter scope in any measured interval.
        const unsigned pair = (dimensions[0] == 1920 ? 0u : 4u) + lists;
        auto expected = observe(f, *owners[0], inputs, false, "cost_frontier", pair);
        auto actual = observe(f, *owners[1], inputs, true, "cost_frontier", pair);
        CHECK(expected.draws == actual.draws); CHECK(expected.commits == actual.commits);
        CHECK(inputs.pixels(*actual.output) == inputs.pixels(*expected.output));
        CHECK(actual.output->depthStencilTexture() == inputs.gbuffer->depthStencilTexture());
        std::cout << "main_lighting_cost_summary width=" << dimensions[0] << " height=" << dimensions[1]
            << " lists=" << lists << " measured_pairs=" << measured
            << " serial_owner_cpu_median_ms=" << median(cpu[0]) << " chain_owner_cpu_median_ms=" << median(cpu[1])
            << " serial_settled_median_ms=" << median(settled[0]) << " chain_settled_median_ms=" << median(settled[1])
            << " serial_gpu_median_ms=" << median(gpu[0]) << " chain_gpu_median_ms=" << median(gpu[1])
            << " measured_settled_win=" << (median(settled[1]) < median(settled[0])) << '\n';
        // Adverse samples/cells remain in the raw output. Native microbenchmark
        // results are not asserted speedups and do not establish viewer FPS.
    }
}
