/** Jittered water reconstruction through the production field, haze and Aurora owners.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalauroraconventions.h"
#include "../almetaltemporalworldhistory.h"
#include <simd/simd.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>

// Frozen-source characterization can compile this fixture against the old
// libraries. The ordinary test must always exercise the production helper.
#ifndef ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
#define ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR 1
#endif

namespace
{
constexpr unsigned WW = 96, WH = 64, WOW = 144, WOH = 96, WP = 18;
using WMatrix = std::array<float, 16>;
using WColor = std::array<float, 4>;
using WHalf = std::array<__fp16, 4>;
WMatrix waterMatrix(simd_float4x4 matrix)
{
    WMatrix result;
    std::memcpy(result.data(), &matrix, sizeof(matrix));
    return result;
}
simd_float4x4 waterSimd(const float* values)
{
    simd_float4x4 result;
    std::memcpy(&result, values, sizeof(result));
    return result;
}
WMatrix waterProjection(unsigned width = WW, unsigned height = WH)
{
    WMatrix p{};
    const float near = .1f, far = 30.f;
    p[0] = 1.4f * float(height) / width; p[5] = 1.4f;
    p[10] = near / (far - near); p[11] = -1.f; p[14] = far * near / (far - near);
    return p;
}
WMatrix waterView(float camera_x)
{
    // Actual perspective camera at (camera_x,-3,2), looking down across the
    // agent-space z=0 water field. Eye Y is up; input rows retain GL clip Y.
    WMatrix v{};
    v[0] = 1.f; v[5] = .6f; v[6] = -.8f; v[9] = .8f; v[10] = .6f;
    v[12] = -camera_x; v[13] = .2f; v[14] = -3.6f; v[15] = 1.f;
    return v;
}
void waterProductionJitter(ALMetalWorldWaterHaze& field, const std::array<float, 2>& jitter)
{
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
    alMetalApplyTemporalWaterJitter(field, jitter[0], jitter[1], WW, WH);
#else
    alMetalApplyTemporalJitter(field.modelview_projection, jitter[0], jitter[1], WW, WH);
#endif
}
template<class T> void waterRaw(const std::filesystem::path& path, const std::vector<T>& values)
{
    std::ofstream stream(path, std::ios::binary); REQUIRE(stream.good());
    stream.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
    REQUIRE(stream.good());
}
std::vector<WColor> waterRead(const ALMetalTexture& texture, ALMetalDevice& device, std::string& error)
{
    REQUIRE(texture.descriptor().format == ALMetalPixelFormat::rgba16_float);
    std::vector<WHalf> half(std::size_t(texture.width()) * texture.height());
    REQUIRE(texture.readback(device, 0, 0, {}, half.data(), texture.width() * 8, &error));
    std::vector<WColor> result(half.size());
    for (unsigned p = 0; p < half.size(); ++p)
        for (unsigned c = 0; c < 4; ++c) result[p][c] = half[p][c];
    return result;
}
struct WaterPixels
{
    std::vector<WColor> raw, opaque, output;
    std::vector<float> depth, color_depth, captured_water_depth;
    std::vector<std::array<__fp16, 2>> motion;
    std::vector<std::uint8_t> unsafe, original_unsafe, reactive, diagnostic_region, water_coverage;
    std::array<float, 2> jitter{};
    unsigned phase = 0, field_draws = 0;
    bool pane_previous_valid = false;
};
struct WaterFixture : Fixture
{
    ALMetalTemporalWorldHistory history;
    std::unique_ptr<ALMetalAuroraReconstructor> aurora;
    std::unique_ptr<ALMetalRenderTarget> hdr, opaque_color, opaque_depth, output;
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
    std::unique_ptr<ALMetalRenderTarget> water_inputs;
#endif
    ALMetalWorldAlphaEnvironment environment;
    ALMetalWorldLegacyAlphaLighting lighting;
    const ALMetalTexture *cells = nullptr, *empty_cells = nullptr, *normal = nullptr, *legacy_cube = nullptr;
    ALMetalWorldSimpleBatch floor, foreground;
    std::uint64_t world = 0;
    std::filesystem::path directory;

    WaterFixture()
    {
        if (const char* value = std::getenv("ALCHEMY_TEMPORAL_WATER_OUTPUT")) directory = value;
        aurora = ALMetalAuroraReconstructor::create(*device, &error); INFO(error); REQUIRE(aurora);
        aurora->setFrameData(data);
        REQUIRE(aurora->prepare({ WW, WH, WOW, WOH }, &error));
        hdr = renderer->prepareTemporalTarget(WW, WH, false, &error);
        opaque_color = renderer->prepareTemporalTarget(WW, WH, false, &error);
        opaque_depth = renderer->prepareTemporalOpaqueDepth(WW, WH, &error);
        output = renderer->prepareTemporalTarget(WOW, WOH, false, &error);
        REQUIRE(hdr); REQUIRE(opaque_color); REQUIRE(opaque_depth); REQUIRE(output);
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
        water_inputs = renderer->prepareTemporalWaterInputs(WW, WH, &error); INFO(error); REQUIRE(water_inputs);
#endif
        const std::array<float, 2> cell{ 0.f, 1.f };
        cells = texture(ALMetalPixelFormat::rg32_float, 1, cell.data(), 8);
        const std::array<float, 2> empty_cell{ 0.f, 0.f };
        empty_cells = texture(ALMetalPixelFormat::rg32_float, 1, empty_cell.data(), 8);
        std::vector<WHalf> normal_pixels(32 * 32), detail(64 * 64);
        for (unsigned y = 0; y < 32; ++y) for (unsigned x = 0; x < 32; ++x)
            normal_pixels[y * 32 + x] = { __fp16(.5f + .12f * std::sin(.51f*x + .17f*y)),
                __fp16(.5f + .11f * std::cos(.29f*x - .43f*y)), __fp16(1.f), __fp16(1.f) };
        normal = texture(ALMetalPixelFormat::rgba16_float, 32, normal_pixels.data(), 32 * 8);
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x)
        {
            const float grid = ((x / 5) % 2) != ((y / 7) % 2) ? .75f : .08f;
            detail[y * 64 + x] = { __fp16(grid), __fp16(.1f + .5f * float(x)/63),
                __fp16(.1f + .5f * float(y)/63), __fp16(1.f) };
        }
        const auto* detail_map = texture(ALMetalPixelFormat::rgba16_float, 64, detail.data(), 64 * 8);
        ALMetalTextureDescriptor cube;
        cube.type = ALMetalTextureType::texture_cube;
        // The actual legacy Environment contract is RGBA8Unorm, even when
        // the scene itself is HDR. Sampling performs its established transfer.
        cube.format = ALMetalPixelFormat::rgba8_unorm;
        cube.usage = ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> c = ALMetalTexture::create(*device, cube, &error); REQUIRE(c);
        auto command = device->makeCommandBuffer("Water fixed legacy sky cube", &error); REQUIRE(command);
        const std::array<std::uint8_t,4> sky{ 31,59,102,255 };
        for (unsigned face = 0; face < 6; ++face) REQUIRE(c->upload(*command, 0, face, {}, sky.data(), 4, &error));
        REQUIRE(command->commitAndWait(&error)); resources->textures.push_back(c); legacy_cube = c.get();
        floor = card(-5.f, 5.f, -3.f, 7.f, -.9f, detail_map, 701);
        // An opaque ledge genuinely above the water, not a test-only screen mask.
        foreground = card(-.65f, .35f, -1.2f, .25f, .45f, color, 702);
        environment.water_plane[1] = .8f; environment.water_plane[2] = .6f; environment.water_plane[3] = 2.f;
        environment.water_fog[0] = 1.f; environment.water_fog[2] = 1.f;
        environment.glow_max_y[1] = 100.f; environment.glow_max_y[3] = 1.f;
        environment.light_direction[1] = .8f; environment.light_direction[2] = .6f;
        environment.atmospheric[1] = environment.atmospheric[2] = environment.atmospheric[3] = 1.f;
        environment.atmos_sunlit[3] = environment.atmos_sunlit_linear[3] = 1.f;
        for (unsigned i = 0; i < 3; ++i)
        {
            environment.eye_to_sky[i][i] = 1.f;
            environment.atmos_sunlit[i] = environment.atmos_sunlit_linear[i] = .3f;
        }
        lighting.sun_direction[1] = .8f; lighting.sun_direction[2] = .6f;
        lighting.parameters[0] = 128.f; lighting.parameters[1] = .004f; lighting.parameters[2] = 1.f;
    }
    ~WaterFixture() { renderer->cancelCommandBatch(); device->waitUntilIdle(); }
    ALMetalWorldSimpleBatch card(float x0, float x1, float y0, float y1, float z, const ALMetalTexture* image, unsigned id)
    {
        Geometry g{};
        const float xy[4][2]{ {x0,y0}, {x1,y0}, {x1,y1}, {x0,y1} };
        for (unsigned i = 0; i < 4; ++i)
        {
            g.positions[i][0] = xy[i][0]; g.positions[i][1] = xy[i][1]; g.positions[i][2] = z; g.positions[i][3] = 1.f;
            g.normals[i][2] = 1.f; g.tangents[i][0] = g.tangents[i][3] = 1.f;
            g.uv[i][0] = i == 1 || i == 2; g.uv[i][1] = i >= 2;
            std::fill_n(g.colors[i], 4, std::uint8_t(255));
        }
        const std::uint16_t indices[6]{0,1,2,0,2,3};
        auto value = batch(0);
        value.vertices = buffer(&g, sizeof(g)); value.indices = buffer(indices, sizeof(indices));
        value.vertex_count = 4; value.index_count = 6;
        value.texture_count = 1; value.diffuse_textures[0] = image;
        value.temporal_instance_id = id; value.temporal_generation = 1;
        identity(value.texture_matrix);
        return value;
    }
    ALMetalWorldWaterSurface water(float camera_x, const WMatrix& p, const WMatrix& v)
    {
        ALMetalWorldWaterSurface w;
        auto& f = w.field;
        f.cell_map = cells; f.above_water = true;
        const auto mvp = waterMatrix(simd_mul(waterSimd(p.data()), waterSimd(v.data())));
        const auto inverse = waterMatrix(simd_inverse(waterSimd(p.data())));
        std::copy(mvp.begin(), mvp.end(), f.modelview_projection);
        std::copy(inverse.begin(), inverse.end(), f.inverse_projection);
        f.cell_square[0] = f.cell_square[1] = -4.f; f.cell_square[2] = 8.f; f.cell_square[3] = 1.f;
        f.rings[0] = 4.f; f.rings[1] = 1.f; f.rings[2] = 4.f;
        std::copy_n(environment.water_plane, 4, f.water_plane);
        f.fog_color[0] = .1f; f.fog_color[1] = .35f; f.fog_color[2] = .65f;
        f.fog[0] = 1.f; f.fog[1] = .25f;
        f.draws[0] = {0,4,1,true}; f.draws[1] = {1,9,12,false}; f.draws[2] = {2,9,4,false}; f.draw_count = 3;
        std::copy(v.begin(), v.end(), w.modelview);
        for (unsigned column = 0; column < 3; ++column)
            for (unsigned row = 0; row < 3; ++row) w.normal_matrix[column][row] = v[column*4+row];
        w.eye_position[0] = camera_x; w.eye_position[1] = -3.f; w.eye_position[2] = 2.f;
        w.normal_maps = { normal, normal }; w.normal_maps_mipped = true;
        w.water_far[0] = 10000.f;
        w.normal_scale_glint[0] = w.normal_scale_glint[1] = w.normal_scale_glint[2] = 2.f;
        w.normal_scale_glint[3] = .35f;
        w.optics[0] = .2f; w.optics[1] = .1f; w.optics[2] = .25f;
        w.parameters[1] = .1f; w.parameters[2] = 1.f;
        w.light_direction[1] = -.6f; w.light_direction[2] = .8f;
        w.underwater = false; w.refracts = true;
        return w;
    }
    ALMetalWorldSimpleBatch transparentPane(float alpha, float height, bool post_water = true)
    {
        const WHalf tint{ __fp16(.85f), __fp16(.13f), __fp16(.28f), __fp16(alpha) };
        const auto* image = texture(ALMetalPixelFormat::rgba16_float, 1, tint.data(), 8);
        auto pane = card(.4f, 2.6f, -1.25f, 1.5f, height, image, 703);
        pane.pre_water_alpha = !post_water;
        pane.post_water_alpha = post_water;
        pane.pre_water_fullbright = true;
        pane.pre_water_minimum_alpha = .004f;
        return pane;
    }
    WaterPixels render(bool independent_inverse = false, float camera_x = 0.f, float animation = 0.f,
        bool include_water = true, bool localized_history_diagnostic = false, bool drawn_cell = true,
        bool use_surface_inputs = true, const ALMetalWorldSimpleBatch* transparent_pane = nullptr,
        bool invalid_pane_previous = false)
    {
        ++world; data->beginFrame(world);
        const auto p = waterProjection(), v = waterView(camera_x);
        const auto camera = waterMatrix(simd_mul(waterSimd(p.data()), waterSimd(v.data())));
        auto opaque_draws = std::array{ floor, foreground };
        for (auto& b : opaque_draws)
        {
            std::copy(v.begin(), v.end(), b.modelview);
            std::copy(camera.begin(), camera.end(), b.modelview_projection);
            for (unsigned column = 0; column < 3; ++column)
                for (unsigned row = 0; row < 3; ++row) b.normal_matrix[column][row] = v[column*4+row];
        }
        ALMetalWorldGBufferScene g;
        g.width = WW; g.height = WH; g.hdr = g.normal_hdr = g.reverse_z = true;
        g.batches = opaque_draws.data(); g.batch_count = opaque_draws.size(); g.source_owner = resources;
        g.material_mip_bias = std::log2(float(WW)/WOW)-1.f;
        auto surface = water(camera_x, p, v);
        surface.field.cell_map = drawn_cell ? cells : empty_cells;
        auto haze = surface.field;
        surface.big_phase[0] = animation;
        surface.little_phase[1] = animation * .7f;
        std::array<ALMetalWorldSimpleBatch, 3> markers{};
        // Match the production above-water refraction ordering: fog what is
        // behind the field before copying it for the surface's refraction.
        markers[0].water_haze = true; markers[1].water_surface = true;
        unsigned forward_count = include_water ? 2 : 0;
        if (transparent_pane)
        {
            auto& pane = markers[forward_count++];
            pane = *transparent_pane;
            std::copy(v.begin(), v.end(), pane.modelview);
            std::copy(camera.begin(), camera.end(), pane.modelview_projection);
            for (unsigned column = 0; column < 3; ++column)
                for (unsigned row = 0; row < 3; ++row) pane.normal_matrix[column][row] = v[column*4+row];
        }
        ALMetalWorldOpaquePostDeferredScene f;
        f.width = WW; f.height = WH; f.hdr = f.reverse_z = true;
        f.batches = markers.data(); f.batch_count = forward_count;
        f.source_owner = resources; f.water_surface = include_water ? &surface : nullptr; f.water_haze = include_water ? &haze : nullptr;
        f.alpha_environment = &environment; f.alpha_lighting = &lighting; f.legacy_environment = legacy_cube;
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
        f.temporal_water_inputs = use_surface_inputs ? water_inputs.get() : nullptr;
#endif
        ALMetalTemporalWorldHistory::FrameInput h;
        h.world_id = world; h.input_width = WW; h.input_height = WH; h.output_width = WOW; h.output_height = WOH;
        h.mode = 8; h.reverse_z = true; h.jitter_phase_count = WP;
        std::copy(camera.begin(), camera.end(), h.camera_mvp);
        REQUIRE(history.beginFrame(h, &error)); REQUIRE(history.stage(g, f, &error));
        WaterPixels pixels; pixels.jitter = history.jitterPixels(); pixels.phase = history.stats().jitter_phase;
        if (transparent_pane && !history.previousForward().empty())
            pixels.pane_previous_valid = history.previousForward().back().valid && !invalid_pane_previous;
        std::copy(pixels.jitter.begin(), pixels.jitter.end(), g.temporal_jitter_pixels);
        waterProductionJitter(surface.field, pixels.jitter); waterProductionJitter(haze, pixels.jitter);
        if (independent_inverse)
        {
            // Independent oracle inverts the whole actually rasterized
            // perspective projection, instead of duplicating J^-1 composition.
            auto raster = p; alMetalApplyTemporalJitter(raster.data(), pixels.jitter[0], pixels.jitter[1], WW, WH);
            const auto inv = waterMatrix(simd_inverse(waterSimd(raster.data())));
            std::copy(inv.begin(), inv.end(), surface.field.inverse_projection);
            std::copy(inv.begin(), inv.end(), haze.inverse_projection);
        }
        ALMetalWorldTemporalPostConfig config;
        config.width = config.presentation_width = WOW; config.height = config.presentation_height = WOH;
        auto targets = renderer->prepareTemporalPostTargets(config, &error); REQUIRE(targets);
        REQUIRE(renderer->beginCommandBatch("Water actual fixed field/haze and input replay", &error));
        auto gbuffer = renderer->render(g, &error); INFO(error); REQUIRE(gbuffer);
        REQUIRE(renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(), *opaque_depth, &error));
        REQUIRE(renderer->resolveTemporalColor(*gbuffer, *gbuffer, *hdr, &error));
        auto descriptor = hdr->descriptor(); descriptor.colors[0].texture = hdr->colorTexture(0);
        descriptor.depth_stencil_format = gbuffer->descriptor().depth_stencil_format;
        descriptor.depth_stencil_shader_read = true; descriptor.depth_stencil_texture = gbuffer->depthStencilTexture();
        auto continuation = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(continuation);
        f.gbuffer = gbuffer.get(); f.composed_lighting = continuation.get(); f.reconstruction_opaque = opaque_color.get();
        auto color_scene = f;
        auto color_batches = markers;
        if (transparent_pane)
        {
            // History/motion retain the authoritative unjittered pose. Color
            // uses the same raster jitter as the opaque and water producers.
            alMetalApplyTemporalJitter(color_batches[forward_count-1].modelview_projection,
                pixels.jitter[0], pixels.jitter[1], WW, WH);
            color_scene.batches = color_batches.data();
        }
        auto final = renderer->renderOpaquePostDeferred(color_scene, std::move(continuation), &error, &pixels.field_draws);
        INFO(error); REQUIRE(final);
        ALMetalWorldTemporalMotionScene m;
        m.width = WW; m.height = WH; m.reverse_z = true; m.opaque_scene = &g; m.forward_scene = &f;
        m.depth = gbuffer->depthStencilTexture(); m.opaque_depth = opaque_depth->colorTexture(0);
        m.previous_opaque = history.previousOpaque().data(); m.previous_opaque_count = history.previousOpaque().size();
        m.previous_forward = history.previousForward().data(); m.previous_forward_count = history.previousForward().size();
        std::vector<ALMetalWorldTemporalPreviousDraw> pane_previous;
        if (transparent_pane && invalid_pane_previous && !history.previousForward().empty())
        {
            pane_previous = history.previousForward();
            pane_previous.back().valid = false;
            m.previous_forward = pane_previous.data();
        }
        m.reset = history.resetRequired(); std::copy(pixels.jitter.begin(), pixels.jitter.end(), m.jitter_pixels);
        std::copy(history.clipToPrevious().begin(), history.clipToPrevious().end(), m.clip_to_previous_clip);
        const auto inverse = waterMatrix(simd_inverse(waterSimd(p.data())));
        std::copy(inverse.begin(), inverse.end(), m.inverse_projection);
        m.reject_water = include_water; std::copy_n(environment.water_plane, 4, m.eye_water_plane);
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
        m.water_inputs = use_surface_inputs ? water_inputs.get() : nullptr;
#endif
        auto motion = renderer->renderTemporalMotion(m, &error); INFO(error); REQUIRE(motion);
        REQUIRE(renderer->commitCommandBatch(&error));
        if (localized_history_diagnostic)
        {
            // TEST ONLY: isolate the effect of the existing force-rejection in
            // stationary, drawn cell interiors. The real water depth/motion
            // remains unchanged, so this is not a valid water-history policy.
            // Readbacks/waits here are diagnostic observation, never production.
            REQUIRE(camera_x == 0.f); REQUIRE(animation == 0.f); REQUIRE(include_water);
            pixels.raw = waterRead(*final->colorTexture(0), *device, error);
            pixels.opaque = waterRead(*opaque_color->colorTexture(0), *device, error);
            pixels.depth.resize(WW*WH); pixels.original_unsafe.resize(WW*WH); pixels.diagnostic_region.resize(WW*WH);
            std::vector<float> opaque_scene_depth(WW*WH);
            REQUIRE(motion->colorTexture(2)->readback(*device,0,0,{},pixels.depth.data(),WW*4,&error));
            REQUIRE(opaque_depth->colorTexture(0)->readback(*device,0,0,{},opaque_scene_depth.data(),WW*4,&error));
            REQUIRE(motion->colorTexture(1)->readback(*device,0,0,{},pixels.original_unsafe.data(),WW,&error));
            auto changed = pixels.original_unsafe;
            const auto eye_to_agent = simd_inverse(waterSimd(v.data()));
            for(unsigned y=3;y+3<WH;++y)for(unsigned x=3;x+3<WW;++x)
            {
                const unsigned i=y*WW+x;
                const simd_float4 clip{2.f*(x+.5f-pixels.jitter[0])/WW-1.f,
                    2.f*(y+.5f-pixels.jitter[1])/WH-1.f,opaque_scene_depth[i],1.f};
                auto eye=simd_mul(waterSimd(inverse.data()),clip); eye/=eye.w;
                const float denominator=.8f*eye.y+.6f*eye.z;
                const float t=-2.f/denominator;
                auto intersection=simd_mul(eye_to_agent,simd_float4{eye.x*t,eye.y*t,eye.z*t,1.f});
                // Authored cell is [-4,4]^2 with its drawn flag set. Exclude a
                // full world unit around its edge, and opaque surfaces in front.
                float contribution=0;
                for(unsigned c=0;c<3;++c)contribution+=std::abs(pixels.raw[i][c]-pixels.opaque[i][c]);
                const bool drawn_interior=!history.resetRequired() && std::isfinite(t) && t>0.f && t<.98f &&
                    std::abs(intersection.x)<3.f && std::abs(intersection.y)<3.f &&
                    std::abs(intersection.z)<.0001f && contribution>.005f;
                if(drawn_interior) { pixels.diagnostic_region[i]=255; changed[i]=0; }
            }
            auto upload=device->makeCommandBuffer("TEST ONLY stationary visible-water rejection isolation",&error); REQUIRE(upload);
            REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(1))->upload(*upload,0,0,{},changed.data(),WW,&error));
            REQUIRE(upload->commitAndWait(&error));
        }
        REQUIRE(renderer->beginMainFrameBatch(world, "Water native Aurora and normal presentation", &error));
        ALMetalAuroraFrame a;
        alMetalAuroraSetViewerCamera(a, p.data(), history.clipToPrevious().data());
        a.jitter_x = pixels.jitter[0]; a.jitter_y = pixels.jitter[1]; a.jitter_phase_count = WP; a.reset = history.resetRequired();
        ALMetalAuroraReconstructor::Ticket ticket = 0;
        REQUIRE(renderer->encodeAuroraUpscale(*aurora, {WW,WH,WOW,WOH}, *final->colorTexture(0),
            *opaque_color->colorTexture(0), *motion, *output, a, ticket, &error));
        ALMetalWorldDisplayPostScene display; display.source = output.get(); display.hdr = true;
        auto displayed = renderer->renderDisplayPost(display, &error, std::move(targets->display)); REQUIRE(displayed);
        ALMetalWorldPresentationScene presentation; presentation.source = displayed.get();
        presentation.output_width = WOW; presentation.output_height = WOH;
        auto presented = renderer->renderPresentationPost(presentation, &error, std::move(targets->presentation)); REQUIRE(presented);
        auto completed = renderer->commandBatchCompletion(); REQUIRE(completed);
        REQUIRE(renderer->observeCommandBatchSettlement([&](bool submitted) {
            if (submitted) { REQUIRE(aurora->submitted(ticket, &error)); REQUIRE(history.commit(world, &error)); }
            else { aurora->abandon(ticket); history.abandon(); }
        }, &error));
        REQUIRE(renderer->commitCommandBatch(&error)); REQUIRE(completed->waitUntilCompleted(&error));
        pixels.raw = waterRead(*final->colorTexture(0), *device, error);
        pixels.opaque = waterRead(*opaque_color->colorTexture(0), *device, error);
        pixels.output = waterRead(*output->colorTexture(0), *device, error);
        pixels.depth.resize(WW*WH); pixels.motion.resize(WW*WH); pixels.unsafe.resize(WW*WH); pixels.reactive.resize(WW*WH);
        pixels.color_depth.resize(WW*WH);
        REQUIRE(final->depthStencilTexture()->readback(*device,0,0,{},pixels.color_depth.data(),WW*4,&error));
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
        if(use_surface_inputs)
        {
            pixels.captured_water_depth.resize(WW*WH); pixels.water_coverage.resize(WW*WH);
            REQUIRE(water_inputs->colorTexture(0)->readback(*device,0,0,{},pixels.captured_water_depth.data(),WW*4,&error));
            REQUIRE(water_inputs->colorTexture(1)->readback(*device,0,0,{},pixels.water_coverage.data(),WW,&error));
        }
#endif
        REQUIRE(motion->colorTexture(2)->readback(*device,0,0,{},pixels.depth.data(),WW*4,&error));
        REQUIRE(motion->colorTexture(0)->readback(*device,0,0,{},pixels.motion.data(),WW*4,&error));
        REQUIRE(motion->colorTexture(1)->readback(*device,0,0,{},pixels.unsafe.data(),WW,&error));
        REQUIRE(aurora->generatedReactive()->readback(*device,0,0,{},pixels.reactive.data(),WW,&error));
        return pixels;
    }
};
double waterError(const std::vector<WColor>& actual, const std::vector<WColor>& expected)
{
    REQUIRE(actual.size() == expected.size());
    double sum = 0;
    for (unsigned p = 0; p < actual.size(); ++p) for (unsigned c = 0; c < 3; ++c)
    { REQUIRE(std::isfinite(actual[p][c])); REQUIRE(std::isfinite(expected[p][c])); sum += std::abs(actual[p][c]-expected[p][c]); }
    return sum / (actual.size()*3);
}
}

TEST_CASE("Temporal water pairs jittered raster and inverse perspective projection", "[almetal][temporal][water][water-jitter]")
{
    for (const auto extent : { std::array<unsigned,2>{WW,WH}, {95,63}, {127,65} })
    {
        const auto p = waterProjection(extent[0],extent[1]);
        const auto inverse = waterMatrix(simd_inverse(waterSimd(p.data())));
        for (unsigned phase = 0; phase < WP; ++phase)
        {
            // Independently specified subpixel locations include asymmetric
            // signs and odd dimensions; this check is not a helper round trip.
            const float jx = float(int(phase*7%19)-9)/19.f, jy = float(int(phase*11%23)-11)/23.f;
            ALMetalWorldWaterHaze field;
            std::copy(p.begin(),p.end(),field.modelview_projection);
            std::copy(inverse.begin(),inverse.end(),field.inverse_projection);
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
            alMetalApplyTemporalWaterJitter(field,jx,jy,extent[0],extent[1]);
#else
            alMetalApplyTemporalJitter(field.modelview_projection,jx,jy,extent[0],extent[1]);
#endif
            for (const auto eye : { simd_float4{.3f,-.2f,-1.1f,1.f}, {-.7f,.1f,-4.3f,1.f}, {.2f,.6f,-12.f,1.f} })
            {
                const auto clip = simd_mul(waterSimd(p.data()),eye);
                const simd_float4 shifted{clip.x/clip.w+2*jx/extent[0],clip.y/clip.w+2*jy/extent[1],clip.z/clip.w,1.f};
                auto restored = simd_mul(waterSimd(field.inverse_projection),shifted); restored /= restored.w;
                CAPTURE(extent[0],extent[1],phase,jx,jy);
                for (unsigned c=0;c<3;++c) CHECK(std::abs(restored[c]-eye[c]) < .000015f);
            }
        }
    }
}

TEST_CASE("Fixed production water haze and refraction match independent jittered inverse through Aurora", "[almetal][temporal][water][water-rendered]")
{
    @autoreleasepool
    {
        WaterFixture actual, expected;
        std::ofstream stats;
        if (!actual.directory.empty())
        {
            std::filesystem::create_directories(actual.directory);
            stats.open(actual.directory/"water-frames.csv");
            stats << "world,phase,jitter_x,jitter_y,camera_x,animation,raw_error,output_error,unsafe_pixels,zero_motion_pixels,draws\n";
        }
        double maximum_raw_error = 0, maximum_output_error = 0, animated_change = 0;
        unsigned rejected_water = 0, accepted_foreground = 0;
        std::array<std::vector<WColor>, WP> fixed_before_animation;
        // Four full warm-up cycles, one stationary cycle, one short translation,
        // two stopped cycles, then genuine normal-map animation is resumed.
        for (unsigned frame = 0; frame < 72+18+18+36+18; ++frame)
        {
            const float camera = frame < 90 ? 0.f : (frame < 108 ? .02f*(frame-89) : .36f);
            const float animation = frame < 144 ? 0.f : .017f*(frame-143);
            const auto a=actual.render(false,camera,animation), b=expected.render(true,camera,animation);
            const double raw_error=waterError(a.raw,b.raw), output_error=waterError(a.output,b.output);
            maximum_raw_error=std::max(maximum_raw_error,raw_error); maximum_output_error=std::max(maximum_output_error,output_error);
            unsigned unsafe=0,zero_motion=0;
            for(unsigned i=0;i<a.unsafe.size();++i)
            {
                unsafe += a.unsafe[i]!=0;
                zero_motion += float(a.motion[i][0])==0.f && float(a.motion[i][1])==0.f;
            }
            if(frame>=72 && frame<90)
            {
                // Projected interior ROIs of the actual water at (1,0,0)
                // and the opaque ledge around (-.15,-.5,.45), away from edges.
                for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)
                    rejected_water += a.unsafe[y*WW+x]!=0;
                for(unsigned y=35;y<38;++y)for(unsigned x=44;x<48;++x)
                    accepted_foreground += a.unsafe[y*WW+x]==0;
            }
            REQUIRE(a.field_draws>=6); REQUIRE(a.phase==b.phase);
            if(frame>=126 && frame<144)fixed_before_animation[a.phase]=a.raw;
            if(frame>=144)animated_change=std::max(animated_change,waterError(a.raw,fixed_before_animation[a.phase]));
            if(stats)stats << std::setprecision(10) << frame << ',' << a.phase << ',' << a.jitter[0] << ',' << a.jitter[1] << ','
                << camera << ',' << animation << ',' << raw_error << ',' << output_error << ',' << unsafe << ',' << zero_motion << ',' << a.field_draws << '\n';
            if(!actual.directory.empty() && frame>=72)
            {
                const auto prefix=actual.directory/("frame-"+std::to_string(frame));
                waterRaw(prefix.string()+"-raw.rgba32f",a.raw); waterRaw(prefix.string()+"-opaque.rgba32f",a.opaque);
                waterRaw(prefix.string()+"-output.rgba32f",a.output); waterRaw(prefix.string()+"-depth.r32f",a.depth);
                waterRaw(prefix.string()+"-motion.rg16f",a.motion); waterRaw(prefix.string()+"-unsafe.r8",a.unsafe);
                waterRaw(prefix.string()+"-reactive.r8",a.reactive);
                waterRaw(prefix.string()+"-color-depth.r32f",a.color_depth);
                if(!a.water_coverage.empty())
                {
                    waterRaw(prefix.string()+"-water-depth.r32f",a.captured_water_depth);
                    waterRaw(prefix.string()+"-water-coverage.r8",a.water_coverage);
                }
            }
        }
        CAPTURE(maximum_raw_error,maximum_output_error,rejected_water,accepted_foreground,animated_change);
        CHECK(maximum_raw_error < .00002);
        CHECK(maximum_output_error < .00003);
        // Full water rejection remains active. This test cannot be made green
        // by globally trusting animated water history or dropping the field.
        CHECK(rejected_water>500); CHECK(accepted_foreground>150);
        CHECK(animated_change>.00002);
    }
}

TEST_CASE("Stationary visible-water interior isolates forced rejection without changing inputs",
          "[almetal][temporal][water][water-rejection-diagnostic]")
{
    @autoreleasepool
    {
        WaterFixture control, diagnostic;
        std::ofstream csv;
        if(!control.directory.empty())
        {
            std::filesystem::create_directories(control.directory);
            csv.open(control.directory/"water-rejection-isolation.csv");
            csv << "world,phase,eligible_inputs,control_variation,diagnostic_variation,control_detail,diagnostic_detail\n";
        }
        std::vector<WColor> previous_control,previous_diagnostic;
        double control_variation=0,diagnostic_variation=0,control_detail=0,diagnostic_detail=0;
        std::size_t observations=0,eligible_total=0;
        for(unsigned frame=0;frame<72+36;++frame)
        {
            const auto a=control.render(),b=diagnostic.render(false,0.f,0.f,true,true);
            // Rejection is the sole intervention. In particular do not change
            // the current water depth/motion into invented surface inputs.
            REQUIRE(a.raw==b.raw); REQUIRE(a.opaque==b.opaque);
            REQUIRE(a.depth==b.depth); REQUIRE(a.motion==b.motion);
            REQUIRE(a.unsafe==b.original_unsafe); REQUIRE(a.phase==b.phase);
            unsigned eligible=0;
            for(unsigned i=0;i<b.unsafe.size();++i)
            {
                if(b.diagnostic_region[i]) { CHECK(b.unsafe[i]==0); ++eligible; }
                else CHECK(b.unsafe[i]==a.unsafe[i]);
            }
            if(frame>=72)
            {
                double av=0,bv=0,ad=0,bd=0;
                unsigned samples=0;
                for(unsigned y=3;y+3<WOH;++y)for(unsigned x=3;x+3<WOW;++x)
                {
                    const unsigned ix=unsigned((x+.5f)*WW/WOW),iy=unsigned((y+.5f)*WH/WOH);
                    bool interior=true;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
                        interior=interior && b.diagnostic_region[(iy+dy)*WW+ix+dx]!=0;
                    if(!interior)continue;
                    const unsigned i=y*WOW+x;
                    for(unsigned c=0;c<3;++c)
                    {
                        av+=std::abs(a.output[i][c]-previous_control[i][c]);
                        bv+=std::abs(b.output[i][c]-previous_diagnostic[i][c]);
                        ad+=std::abs(a.output[i][c]-a.output[i+1][c]);
                        bd+=std::abs(b.output[i][c]-b.output[i+1][c]);
                    }
                    samples+=3;
                }
                REQUIRE(samples>100);
                av/=samples;bv/=samples;ad/=samples;bd/=samples;
                control_variation+=av;diagnostic_variation+=bv;control_detail+=ad;diagnostic_detail+=bd;
                ++observations;eligible_total+=eligible;
                if(csv)csv << std::setprecision(10) << frame << ',' << a.phase << ',' << eligible << ','
                    << av << ',' << bv << ',' << ad << ',' << bd << '\n';
                if(!control.directory.empty())
                {
                    const auto prefix=control.directory/("isolation-"+std::to_string(frame));
                    waterRaw(prefix.string()+"-control.rgba32f",a.output);
                    waterRaw(prefix.string()+"-diagnostic.rgba32f",b.output);
                    waterRaw(prefix.string()+"-original-unsafe.r8",a.unsafe);
                    waterRaw(prefix.string()+"-used-unsafe.r8",b.unsafe);
                    waterRaw(prefix.string()+"-eligible.r8",b.diagnostic_region);
                }
            }
            previous_control=a.output;previous_diagnostic=b.output;
        }
        REQUIRE(observations==36); REQUIRE(eligible_total>3600);
        control_variation/=observations;diagnostic_variation/=observations;
        control_detail/=observations;diagnostic_detail/=observations;
        // Observations, not a proposed policy acceptance condition. More
        // stable output here would not qualify animated-water history.
        SUCCEED("TEST ONLY visible-water rejection isolation: control variation=" << control_variation
            << ", diagnostic=" << diagnostic_variation << ", control detail=" << control_detail
            << ", diagnostic=" << diagnostic_detail << ", eligible inputs=" << eligible_total);
    }
}

TEST_CASE("Temporal water inputs follow actual color coverage and foreground depth",
          "[almetal][temporal][water][water-inputs]")
{
    @autoreleasepool
    {
        WaterFixture visible, hole, absent;
        double maximum_water_depth_error=0,maximum_water_camera_motion_error=0;
        unsigned actual_water=0,uncovered_floor=0,protected_foreground=0;
        const auto projection=waterProjection();
        const auto inverse=waterSimd(waterMatrix(simd_inverse(waterSimd(projection.data()))).data());
        for(unsigned frame=0;frame<8;++frame)
        {
            const float camera=frame<2 ? 0.f : .02f;
            const float previous_camera=frame<3 ? 0.f : .02f;
            const float animation=frame<4 ? 0.f : .017f*(frame-3);
            const auto a=visible.render(false,camera,animation),b=hole.render(false,camera,animation,true,false,false),
                c=absent.render(false,camera,animation,false);
            REQUIRE(a.phase==b.phase); REQUIRE(a.phase==c.phase);
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
            REQUIRE(c.water_coverage.size()==WW*WH); REQUIRE(c.captured_water_depth.size()==WW*WH);
            for(unsigned i=0;i<WW*WH;++i)
            { CHECK(c.water_coverage[i]==0); CHECK(c.captured_water_depth[i]==0.f); }
#endif
            if(frame==0)continue; // First-use reset is independently unsafe.
            for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)
            {
                const auto i=y*WW+x;
                ++actual_water;
                maximum_water_depth_error=std::max(maximum_water_depth_error,double(std::abs(a.depth[i]-a.color_depth[i])));
                // This point's color is an actual water-field surface in
                // front of the floor, not a water-plane classification guess.
                CHECK(a.color_depth[i]>b.color_depth[i]+.0001f);
                CHECK(a.unsafe[i]==255);
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
                REQUIRE(a.water_coverage[i]==255); REQUIRE(b.water_coverage[i]==0);
                CHECK(a.captured_water_depth[i]==a.color_depth[i]);
#endif
                const float uvx=(x+.5f-a.jitter[0])/WW,uvy=(y+.5f-a.jitter[1])/WH;
                auto eye=simd_mul(inverse,simd_float4{uvx*2-1,uvy*2-1,a.color_depth[i],1.f}); eye/=eye.w;
                auto previous_eye=eye; previous_eye.x+=camera-previous_camera;
                const auto previous_clip=simd_mul(waterSimd(projection.data()),previous_eye);
                const float expected_x=(previous_clip.x/previous_clip.w*.5f+.5f-uvx)*WW;
                const float expected_y=(previous_clip.y/previous_clip.w*.5f+.5f-uvy)*WH;
                maximum_water_camera_motion_error=std::max(maximum_water_camera_motion_error,
                    double(std::max(std::abs(float(a.motion[i][0])-expected_x),std::abs(float(a.motion[i][1])-expected_y))));
                ++uncovered_floor;
                // Cell flag0 actually removes the field from this pixel. A
                // hypothetical infinite water plane must not reject its floor.
                CHECK(b.unsafe[i]==0); CHECK(b.depth[i]==c.depth[i]);
                for(unsigned channel=0;channel<4;++channel) CHECK(b.raw[i][channel]==c.raw[i][channel]);
            }
            for(unsigned y=35;y<38;++y)for(unsigned x=44;x<48;++x)
            {
                const auto i=y*WW+x;
                ++protected_foreground;
                CHECK(a.unsafe[i]==0); CHECK(a.depth[i]==a.color_depth[i]);
                for(unsigned channel=0;channel<4;++channel) CHECK(a.raw[i][channel]==c.raw[i][channel]);
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
                CHECK(a.water_coverage[i]==0);
#endif
            }
        }
        CAPTURE(maximum_water_depth_error,maximum_water_camera_motion_error,actual_water,uncovered_floor,protected_foreground);
        CHECK(actual_water==245); CHECK(uncovered_floor==245); CHECK(protected_foreground==84);
        CHECK(maximum_water_depth_error<.000002);
        CHECK(maximum_water_camera_motion_error<.00015);
    }
}

TEST_CASE("Actual water coverage and depth isolate the Temporal input policy with identical color",
          "[almetal][temporal][water][water-input-policy]")
{
    @autoreleasepool
    {
        WaterFixture plane_policy,surface_policy;
        std::ofstream csv;
        if(!surface_policy.directory.empty())
        {
            std::filesystem::create_directories(surface_policy.directory);
            csv.open(surface_policy.directory/"water-input-policy.csv");
            csv << "world,phase,water_outputs,plane_variation,surface_variation,plane_detail,surface_detail,water_depth_error,unsafe_water\n";
        }
        std::vector<WColor> previous_plane,previous_surface;
        double plane_variation=0,surface_variation=0,plane_detail=0,surface_detail=0,maximum_depth_error=0;
        std::size_t observations=0;
        for(unsigned frame=0;frame<72+36;++frame)
        {
            const auto a=plane_policy.render(false,0.f,0.f,true,false,true,false),b=surface_policy.render();
            REQUIRE(a.raw==b.raw); REQUIRE(a.opaque==b.opaque); REQUIRE(a.color_depth==b.color_depth);
            REQUIRE(a.phase==b.phase); REQUIRE(a.jitter==b.jitter);
            if(frame>=72)
            {
                double av=0,bv=0,ad=0,bd=0,depth_error=0;
                unsigned samples=0,unsafe_water=0;
                // Fixed projected cell interior from the independently
                // authored geometry. No water confidence is cleared in this arm.
                for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)
                {
                    const unsigned i=y*WW+x;
                    unsafe_water+=b.unsafe[i]==255;
                    depth_error=std::max(depth_error,double(std::abs(b.depth[i]-b.color_depth[i])));
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
                    REQUIRE(b.water_coverage[i]==255);
#endif
                }
                for(unsigned y=50;y<56;++y)for(unsigned x=90;x<99;++x)
                {
                    const unsigned i=y*WOW+x;
                    for(unsigned c=0;c<3;++c)
                    {
                        av+=std::abs(a.output[i][c]-previous_plane[i][c]);
                        bv+=std::abs(b.output[i][c]-previous_surface[i][c]);
                        ad+=std::abs(a.output[i][c]-a.output[i+1][c]);
                        bd+=std::abs(b.output[i][c]-b.output[i+1][c]);
                    }
                    samples+=3;
                }
                REQUIRE(samples==162); CHECK(unsafe_water==35);
                av/=samples;bv/=samples;ad/=samples;bd/=samples;
                plane_variation+=av;surface_variation+=bv;plane_detail+=ad;surface_detail+=bd;
                maximum_depth_error=std::max(maximum_depth_error,depth_error);++observations;
                if(csv)csv << std::setprecision(10) << frame << ',' << b.phase << ',' << samples/3 << ','
                    << av << ',' << bv << ',' << ad << ',' << bd << ',' << depth_error << ',' << unsafe_water << '\n';
                if(!surface_policy.directory.empty())
                {
                    const auto prefix=surface_policy.directory/("input-policy-"+std::to_string(frame));
                    waterRaw(prefix.string()+"-plane.rgba32f",a.output);waterRaw(prefix.string()+"-surface.rgba32f",b.output);
                    waterRaw(prefix.string()+"-plane-depth.r32f",a.depth);waterRaw(prefix.string()+"-surface-depth.r32f",b.depth);
                    waterRaw(prefix.string()+"-color-depth.r32f",b.color_depth);
                    waterRaw(prefix.string()+"-plane-unsafe.r8",a.unsafe);waterRaw(prefix.string()+"-surface-unsafe.r8",b.unsafe);
                    waterRaw(prefix.string()+"-water-coverage.r8",b.water_coverage);
                }
            }
            previous_plane=a.output;previous_surface=b.output;
        }
        CAPTURE(maximum_depth_error);
        CHECK(maximum_depth_error<.000002); REQUIRE(observations==36);
        // Observational quality comparison, not a softened quality bound. The
        // acceptance assertions above cover actual depth/visibility correctness.
        SUCCEED("Water-input policy: plane variation=" << plane_variation/observations
            << ", actual surface=" << surface_variation/observations << ", plane detail=" << plane_detail/observations
            << ", actual surface=" << surface_detail/observations);
    }
}

#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
TEST_CASE("Post-water transparent coverage preserves visible water and valid full foreground history",
          "[almetal][temporal][water][water-forward-visibility]")
{
    @autoreleasepool
    {
        WaterFixture baseline, zero, partial, full, behind, dry, behind_dry;
        // The authored camera is above water. Match fillMetalAlphaEnvironment:
        // the post-water pool draws the camera side; the pre-water pool draws
        // the submerged side. The old water-only cases retain their inputs.
        for (auto* fixture : { &baseline, &zero, &partial, &full, &behind, &dry, &behind_dry })
            fixture->environment.water_fog[2] = -1.f;
        const auto zero_pane = zero.transparentPane(0.f, .4f);
        const auto partial_pane = partial.transparentPane(.5f, .4f);
        const auto full_pane = full.transparentPane(1.f, .4f);
        const auto behind_pane = behind.transparentPane(1.f, -.35f);
        // A separate, correctly selected dry/pre-water pool proves the
        // submerged pane is actual visible geometry rather than a culled card.
        const auto dry_pane = behind_dry.transparentPane(1.f, -.35f, false);
        const auto foreground_depth = [](const WaterPixels& p, unsigned x, unsigned y)
        {
            const auto projection = waterProjection();
            const auto inverse = simd_inverse(waterSimd(projection.data()));
            const simd_float4 clip{ 2.f*(x+.5f-p.jitter[0])/WW-1.f,
                2.f*(y+.5f-p.jitter[1])/WH-1.f, .5f, 1.f };
            auto ray = simd_mul(inverse, clip); ray /= ray.w;
            // In this independently authored camera, agentZ=.8*eyeY+
            // .6*eyeZ+2. Intersect its ray with the actual pane agentZ=.4.
            const float t = (.4f-2.f)/(.8f*ray.y+.6f*ray.z);
            const simd_float4 eye{ ray.x*t, ray.y*t, ray.z*t, 1.f };
            const auto projected = simd_mul(waterSimd(projection.data()), eye);
            return projected.z/projected.w;
        };
        double maximum_depth_error = 0, maximum_blend_error = 0, full_contribution = 0, dry_contribution = 0;
        unsigned observations = 0;
        for (unsigned frame = 0; frame < 4; ++frame)
        {
            const auto a = baseline.render();
            const auto z = zero.render(false,0,0,true,false,true,true,&zero_pane);
            const auto p = partial.render(false,0,0,true,false,true,true,&partial_pane);
            const auto f = full.render(false,0,0,true,false,true,true,&full_pane);
            const auto b = behind.render(false,0,0,true,false,true,true,&behind_pane);
            const auto d = dry.render(false,0,0,false);
            const auto bd = behind_dry.render(false,0,0,false,false,true,true,&dry_pane);
            if (frame==2 && !baseline.directory.empty())
            {
                const auto directory=baseline.directory/"water-pane-visibility";
                std::filesystem::create_directories(directory);
                const auto save=[&](const char* name,const WaterPixels& value)
                {
                    const auto prefix=directory/(std::string(name)+"-phase"+std::to_string(value.phase));
                    waterRaw(prefix.string()+"-raw.rgba32f",value.raw);
                    waterRaw(prefix.string()+"-output.rgba32f",value.output);
                    waterRaw(prefix.string()+"-depth.r32f",value.depth);
                    waterRaw(prefix.string()+"-motion.rg16f",value.motion);
                    waterRaw(prefix.string()+"-unsafe.r8",value.unsafe);
                    waterRaw(prefix.string()+"-water-depth.r32f",value.captured_water_depth);
                    waterRaw(prefix.string()+"-water-coverage.r8",value.water_coverage);
                };
                save("water",a); save("zero",z); save("partial",p); save("full",f);
                save("behind",b); save("dry",d); save("dry-behind",bd);
            }
            REQUIRE(a.phase==z.phase); REQUIRE(a.phase==p.phase); REQUIRE(a.phase==f.phase); REQUIRE(a.phase==b.phase);
            REQUIRE(a.jitter==z.jitter); REQUIRE(a.jitter==p.jitter); REQUIRE(a.jitter==f.jitter); REQUIRE(a.jitter==b.jitter);
            if (frame < 2) continue;
            REQUIRE(z.pane_previous_valid); REQUIRE(p.pane_previous_valid);
            REQUIRE(f.pane_previous_valid); REQUIRE(b.pane_previous_valid); REQUIRE(bd.pane_previous_valid);
            for (unsigned y=33;y<38;++y) for (unsigned x=59;x<66;++x)
            {
                const unsigned i=y*WW+x;
                ++observations;
                REQUIRE(a.water_coverage[i]==255);
                CHECK(z.water_coverage[i]==255); CHECK(p.water_coverage[i]==255);
                CHECK(f.water_coverage[i]==255); CHECK(b.water_coverage[i]==255);
                CHECK(a.unsafe[i]==255); CHECK(z.unsafe[i]==255);
                CHECK(p.unsafe[i]==255); CHECK(f.unsafe[i]==0); CHECK(b.unsafe[i]==255);
                CHECK(z.depth[i]==a.depth[i]); CHECK(z.motion[i]==a.motion[i]);
                CHECK(b.depth[i]==a.depth[i]); CHECK(b.motion[i]==a.motion[i]);
                const float expected=foreground_depth(f,x,y);
                REQUIRE(expected>a.captured_water_depth[i]+.0001f);
                maximum_depth_error=std::max(maximum_depth_error,
                    double(std::max(std::abs(f.depth[i]-expected),std::abs(p.depth[i]-expected))));
                CHECK(f.motion[i][0]==0); CHECK(f.motion[i][1]==0);
                CHECK(p.motion[i][0]==0); CHECK(p.motion[i][1]==0);
                // Post-water color is depth-read-only. Its nearer Temporal
                // proxy is intentionally distinct from the water attachment.
                CHECK(f.color_depth[i]==a.color_depth[i]); CHECK(p.color_depth[i]==a.color_depth[i]);
                for (unsigned c=0;c<3;++c)
                {
                    CHECK(z.raw[i][c]==a.raw[i][c]); CHECK(b.raw[i][c]==a.raw[i][c]);
                    full_contribution+=std::abs(f.raw[i][c]-a.raw[i][c]);
                    dry_contribution+=std::abs(bd.raw[i][c]-d.raw[i][c]);
                    maximum_blend_error=std::max(maximum_blend_error,
                        double(std::abs(p.raw[i][c]-(f.raw[i][c]+a.raw[i][c])*.5f)));
                }
            }
        }
        REQUIRE(observations==70); REQUIRE(full_contribution>.1); REQUIRE(dry_contribution>.1);
        CAPTURE(maximum_depth_error,maximum_blend_error);
        CHECK(maximum_depth_error<.000002); // Existing actual-water depth tolerance.
        CHECK(maximum_blend_error<.002); // Four half-float ULPs at unit radiance; not a temporal-quality bound.
        std::cout << "Actual water+pane visibility: samples=" << observations
            << ", maximum depth error=" << maximum_depth_error << ", maximum blend error=" << maximum_blend_error
            << ", full RGB contribution=" << full_contribution << ", dry RGB contribution=" << dry_contribution << '\n';

        const auto resets=full.history.stats().resets;
        const auto absent=baseline.render();
        const auto invalid=full.render(false,0,0,true,false,true,true,&full_pane,true);
        REQUIRE(absent.phase==invalid.phase); REQUIRE_FALSE(invalid.pane_previous_valid);
        for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)
        {
            const auto i=y*WW+x;
            CHECK(invalid.unsafe[i]==255); // Full opacity does not grant history to an invalid previous pose.
            CHECK(std::abs(invalid.depth[i]-foreground_depth(invalid,x,y))<.000002);
        }
        const auto recovered=full.render(false,0,0,true,false,true,true,&full_pane);
        REQUIRE(recovered.pane_previous_valid);
        for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)CHECK(recovered.unsafe[y*WW+x]==0);
        CHECK(full.history.stats().resets==resets);

        // Every render above settles through its ordinary completion before
        // returning. Separate warm-up from steady fixed-descriptor worlds so
        // legitimate initial allocation is not confused with target churn.
        const auto before_warm=alMetalResourceAllocationStats().textures_created;
        for(unsigned i=0;i<8;++i)full.render(false,0,0,true,false,true,true,&full_pane);
        const auto warm_created=alMetalResourceAllocationStats().textures_created;
        const auto warm_pool=full.renderer->renderTargetPoolStats();
        for(unsigned i=0;i<8;++i)
        {
            const auto settled=full.render(false,0,0,true,false,true,true,&full_pane);
            REQUIRE(settled.pane_previous_valid);
            CHECK(alMetalResourceAllocationStats().textures_created==warm_created);
        }
        const auto settled_pool=full.renderer->renderTargetPoolStats();
        const auto settled_created=alMetalResourceAllocationStats().textures_created;
        CAPTURE(warm_created,settled_created,warm_pool.cached_bytes,settled_pool.cached_bytes,
            warm_pool.dependency_deferrals,settled_pool.dependency_deferrals);
        CHECK(settled_pool.reuses>warm_pool.reuses);
        CHECK(settled_pool.cached_bytes==warm_pool.cached_bytes);
        std::cout << "Actual water+pane fixed8+8 worlds: warm textures=" << warm_created-before_warm
            << ", steady textures=" << settled_created-warm_created
            << ", pool reuse delta=" << settled_pool.reuses-warm_pool.reuses
            << ", deferral delta=" << settled_pool.dependency_deferrals-warm_pool.dependency_deferrals
            << ", cached bytes=" << settled_pool.cached_bytes << '\n';
    }
}
#endif
