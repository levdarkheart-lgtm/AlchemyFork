/** Stationary transparent panes through actual color/motion/reconstruction.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalauroraconventions.h"
#include "../almetaltemporalworldhistory.h"
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <simd/simd.h>

namespace
{
constexpr unsigned GW = 128, GH = 64, GOW = 192, GOH = 96, GPhases = 18, GWarm = 108, GStable = 54;
using GColor = std::array<float, 4>;
using GHalf  = std::array<__fp16, 4>;
enum class GlassFamily
{
    legacy,
    pbr_single,
    pbr_multi
};
const char* familyName(GlassFamily family)
{
    switch (family)
    {
        case GlassFamily::legacy:
            return "legacy";
        case GlassFamily::pbr_single:
            return "pbr-single";
        case GlassFamily::pbr_multi:
            return "pbr-multi";
    }
    return "unknown";
}
struct GlassSettings
{
    float camera_x = 0.f, pane_x = 0.f, wall_x = 0.f;
    float uv_shift = 0.f;
    bool  overlap = false, invalid_previous = false, zero_alpha = false, glare = false;
};
struct GlassPixels
{
    std::vector<GColor>                color, opaque, output;
    std::vector<float>                 depth;
    std::vector<std::array<__fp16, 2>> motion;
    std::vector<std::uint8_t>          actual_rejection, used_rejection, generated_reactivity;
    std::array<float, 2>               jitter{};
    unsigned                           phase = 0, valid_previous = 0;
};
template<class T>
void glassRaw(const std::filesystem::path& path, const std::vector<T>& pixels)
{
    std::ofstream f(path, std::ios::binary);
    REQUIRE(f.good());
    f.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(T));
    REQUIRE(f.good());
}
std::vector<GColor> glassColors(const ALMetalTexture& t, const ALMetalDevice& device, std::string& error)
{
    REQUIRE(t.descriptor().format == ALMetalPixelFormat::rgba16_float);
    std::vector<GHalf> half(std::size_t(t.width()) * t.height());
    REQUIRE(t.readback(device, 0, 0, {}, half.data(), t.width() * 8, &error));
    INFO(error);
    std::vector<GColor> out(half.size());
    for (std::size_t p = 0; p < out.size(); ++p)
        for (unsigned c = 0; c < 4; ++c)
            out[p][c] = half[p][c];
    return out;
}
struct GlassFixture : Fixture
{
    struct Geometry
    {
        float        positions[8][4]{}, normals[8][4]{}, uv[8][2]{};
        std::uint8_t colors[8][4]{};
        float        tangents[8][4]{}, weights[8][4]{}, cloth[8][4]{};
    };
    ALMetalTemporalWorldHistory                 history;
    std::unique_ptr<ALMetalAuroraReconstructor> aurora;
    std::unique_ptr<ALMetalTemporalScaler>      metalfx;
    std::unique_ptr<ALMetalRenderTarget>        hdr, opaque_color, opaque_depth, reconstructed, resolved;
    std::unique_ptr<ALMetalTexture>             rejection_snapshot;
    ALMetalWorldAlphaEnvironment                environment;
    ALMetalWorldLegacyAlphaLighting             lighting;
    const ALMetalTexture *neutral_cube = nullptr, *bright_cube = nullptr, *wall_texture = nullptr, *pane_texture = nullptr,
                         *white = nullptr;
    ALMetalWorldSimpleBatch wall, pane, second, glare_pane;
    GlassFamily             family;
    bool                    use_metalfx = false;
    float                   opacity     = .35f;
    GlassSettings           previous_settings{};
    bool                    has_previous_settings = false;
    std::filesystem::path   output_dir;
    std::uint64_t           world_id = 0;

    static std::array<float, 16> projection()
    {
        std::array<float, 16> p{};
        p[0]  = .5f;
        p[5]  = 1.f;
        p[11] = -1.f;
        p[14] = .1f;
        return p;
    }
    explicit GlassFixture(GlassFamily f, bool mfx = false, float alpha = .35f) : family(f), use_metalfx(mfx), opacity(alpha)
    {
        if (const char* p = std::getenv("ALCHEMY_TEMPORAL_GLASS_OUTPUT"))
            output_dir = p;
        aurora = ALMetalAuroraReconstructor::create(*device, &error);
        REQUIRE(aurora);
        INFO(error);
        aurora->setFrameData(data);
        ALMetalAuroraDescriptor ad{ GW, GH, GOW, GOH };
        REQUIRE(aurora->prepare(ad, &error));
        if (use_metalfx)
        {
            metalfx = ALMetalTemporalScaler::create(*device, &error);
            REQUIRE(metalfx);
            ALMetalTemporalScalerRequirements r;
            REQUIRE(metalfx->prepare({ GW, GH, GOW, GOH }, r, &error));
        }
        hdr           = renderer->prepareTemporalTarget(GW, GH, false, &error);
        opaque_color  = renderer->prepareTemporalTarget(GW, GH, false, &error);
        opaque_depth  = renderer->prepareTemporalOpaqueDepth(GW, GH, &error);
        reconstructed = renderer->prepareTemporalTarget(GOW, GOH, use_metalfx, &error);
        resolved      = renderer->prepareTemporalTarget(GOW, GOH, false, &error);
        REQUIRE(hdr);
        REQUIRE(opaque_color);
        REQUIRE(opaque_depth);
        REQUIRE(reconstructed);
        REQUIRE(resolved);
        ALMetalTextureDescriptor rd;
        rd.width           = GW;
        rd.height          = GH;
        rd.format          = ALMetalPixelFormat::r8_unorm;
        rd.usage           = ALMetalTextureUsage::shader_read;
        rejection_snapshot = ALMetalTexture::create(*device, rd, &error);
        REQUIRE(rejection_snapshot);
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
        {
            environment.eye_to_sky[i][i]       = 1.f;
            environment.atmos_sunlit[i]        = .25f;
            environment.atmos_sunlit_linear[i] = .2f;
        }
        lighting.sun_direction[2] = 1.f;
        lighting.parameters[0]    = 128.f;
        lighting.parameters[1]    = .004f;
        lighting.parameters[2]    = 1.f;
        neutral_cube              = cube({ 0, 0, 0, 255 });
        bright_cube               = cube({ 255, 230, 210, 255 });
        const std::uint8_t w[4]{ 255, 255, 255, 255 };
        white            = texture(ALMetalPixelFormat::rgba8_unorm, 1, w, 4);
        wall_texture     = detail();
        pane_texture     = patternedPane(alpha);
        wall             = card(901, 0, -4.f, -7.9f, 7.9f, -3.9f, 3.9f, wall_texture, false);
        const unsigned n = family == GlassFamily::legacy ? 1 : 2;
        pane             = card(902, n, -2.f, -3.2f, .8f, -1.6f, 1.6f, pane_texture, true, family == GlassFamily::pbr_multi);
        if (family == GlassFamily::legacy)
            glare_pane = card(902, 1, -2.f, -3.2f, .8f, -1.6f, 1.6f, flatColor({ .13f, .4f, .6f, 0 }), true, false, 128);
        second = card(903, 0, -2.5f, -2.2f, .2f, -1.4f, 1.4f, flatColor({ .65f, .12f, .1f, .35f }), true);
    }
    ~GlassFixture()
    {
        renderer->cancelCommandBatch();
        device->waitUntilIdle();
    }
    const ALMetalTexture* cube(std::array<std::uint8_t, 4> value)
    {
        ALMetalTextureDescriptor d;
        d.type                            = ALMetalTextureType::texture_cube;
        d.format                          = ALMetalPixelFormat::rgba8_unorm;
        d.usage                           = ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> t = ALMetalTexture::create(*device, d, &error);
        REQUIRE(t);
        auto c = device->makeCommandBuffer("Glass authored neutral/specular environment", &error);
        REQUIRE(c);
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(t->upload(*c, 0, face, {}, value.data(), 4, &error));
        REQUIRE(c->commitAndWait(&error));
        resources->textures.push_back(t);
        return t.get();
    }
    const ALMetalTexture* flatColor(GColor value)
    {
        GHalf half;
        std::copy(value.begin(), value.end(), half.begin());
        return texture(ALMetalPixelFormat::rgba16_float, 1, half.data(), 8);
    }
    const ALMetalTexture* patternedPane(float alpha)
    {
        std::vector<GHalf> pixels(32 * 32);
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; ++x)
            {
                const float t = .5f + .5f * std::sin(x * .37f + y * .13f);
                auto&       p = pixels[y * 32 + x];
                p[0]          = .1f + .06f * t;
                p[1]          = .32f + .12f * t;
                p[2]          = .5f + .12f * t;
                p[3]          = alpha * (.85f + .15f * t);
            }
        return texture(ALMetalPixelFormat::rgba16_float, 32, pixels.data(), 32 * 8);
    }
    const ALMetalTexture* detail()
    {
        std::vector<std::array<std::uint8_t, 4>> pixels(256 * 256);
        for (unsigned y = 0; y < 256; ++y)
            for (unsigned x = 0; x < 256; ++x)
            {
                const float a       = .5f + .32f * std::sin(x * .73f + y * .19f) * std::cos(y * .53f);
                pixels[y * 256 + x] = { std::uint8_t(255 * a), std::uint8_t(255 * (.2f + .6f * a)), std::uint8_t(255 * (.8f - .6f * a)),
                                        255 };
            }
        ALMetalTextureDescriptor d;
        d.width = d.height                = 256;
        d.format                          = ALMetalPixelFormat::rgba8_unorm;
        d.mipmap_level_count              = 9;
        d.usage                           = ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> t = ALMetalTexture::create(*device, d, &error);
        REQUIRE(t);
        auto c = device->makeCommandBuffer("Glass independently authored background mip chain", &error);
        REQUIRE(c);
        unsigned extent = 256;
        for (unsigned level = 0; level < 9; ++level)
        {
            REQUIRE(t->upload(*c, level, 0, {}, pixels.data(), extent * 4, &error));
            if (extent == 1)
                break;
            std::vector<std::array<std::uint8_t, 4>> next(extent * extent / 4);
            for (unsigned y = 0; y < extent / 2; ++y)
                for (unsigned x = 0; x < extent / 2; ++x)
                    for (unsigned k = 0; k < 4; ++k)
                    {
                        unsigned sum = 0;
                        for (unsigned dy = 0; dy < 2; ++dy)
                            for (unsigned dx = 0; dx < 2; ++dx)
                                sum += pixels[(2 * y + dy) * extent + 2 * x + dx][k];
                        next[y * (extent / 2) + x][k] = (sum + 2) / 4;
                    }
            pixels = std::move(next);
            extent /= 2;
        }
        REQUIRE(c->commitAndWait(&error));
        resources->textures.push_back(t);
        return t.get();
    }
    ALMetalWorldSimpleBatch card(unsigned id, unsigned material, float z, float l, float r, float b, float t, const ALMetalTexture* image,
                                 bool blend, bool multi = false, std::uint8_t vertex_alpha = 255)
    {
        Geometry       g;
        const unsigned rectangles = multi ? 2 : 1;
        for (unsigned rect = 0; rect < rectangles; ++rect)
        {
            const float left = rect ? (.5f * (l + r)) : l, right = multi && !rect ? (.5f * (l + r)) : r;
            const float xy[4][2]{ { left, b }, { right, b }, { right, t }, { left, t } };
            for (unsigned v = 0; v < 4; ++v)
            {
                const auto i      = rect * 4 + v;
                g.positions[i][0] = xy[v][0];
                g.positions[i][1] = xy[v][1];
                g.positions[i][2] = z;
                g.positions[i][3] = std::bit_cast<float>(std::int32_t(rect));
                g.normals[i][2]   = 1.f;
                g.uv[i][0]        = (v == 1 || v == 2);
                g.uv[i][1]        = v >= 2;
                std::fill_n(g.colors[i], 4, std::uint8_t(255));
                g.colors[i][3]   = vertex_alpha;
                g.tangents[i][0] = g.tangents[i][3] = 1.f;
            }
        }
        const std::array<std::uint16_t, 12> indices{ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };
        auto                                value = batch(material);
        value.vertices                            = buffer(&g, sizeof(g));
        value.indices                             = buffer(indices.data(), rectangles * 6 * 2);
        value.vertex_count                        = rectangles * 4;
        value.index_count                         = rectangles * 6;
        value.normal_offset                       = offsetof(Geometry, normals);
        value.texcoord_offset                     = offsetof(Geometry, uv);
        value.color_offset                        = offsetof(Geometry, colors);
        value.tangent_offset                      = offsetof(Geometry, tangents);
        value.weight_offset                       = 0;
        value.cloth_offset                        = 0;
        value.texture_count                       = rectangles;
        for (unsigned s = 0; s < rectangles; ++s)
            value.diffuse_textures[s] = image;
        value.temporal_instance_id = id;
        value.temporal_generation  = 1;
        identity(value.texture_matrix);
        value.post_water_alpha        = blend;
        value.pre_water_fullbright    = blend && !material;
        value.pre_water_material      = blend && material == 1;
        value.pre_water_pbr           = blend && material == 2;
        value.pre_water_minimum_alpha = .004f;
        if (material == 1)
        {
            value.material_alpha_mode = 1;
            auto& s                   = value.materialSlots()[0];
            s.alpha_cutoff            = -1.f;
            s.fullbright              = 1.f;
            s.env_intensity           = 0;
            std::fill_n(s.specular_color, 4, 0.f);
        }
        if (material == 2)
        {
            value.pbr_slot_count = rectangles;
            for (unsigned slot = 0; slot < rectangles; ++slot)
            {
                auto& s            = value.pbrSlots()[slot];
                s                  = value.pbrSlots()[0];
                s.alpha_cutoff     = -1.f;
                s.metallic_factor  = 0.f;
                s.roughness_factor = 1.f;
                s.emissive_texture = image;
                for (unsigned c = 0; c < 3; ++c)
                    s.emissive_color[c] = .8f;
                if (slot)
                {
                    s.texture_transform[0][0] = 1.3f;
                    s.texture_transform[0][1] = .75f;
                    s.texture_transform[0][4] = .19f;
                    s.texture_transform[0][5] = -.07f;
                }
            }
        }
        return value;
    }
    static void transform(ALMetalWorldSimpleBatch& value, float x, float camera)
    {
        identity(value.modelview);
        value.modelview[12] = x - camera;
        const auto p        = projection();
        std::copy(p.begin(), p.end(), value.modelview_projection);
        value.modelview_projection[12] = p[0] * (x - camera);
    }
    GlassPixels render(GlassSettings settings = {}, bool diagnostic_composition_zero = false)
    {
        ++world_id;
        data->beginFrame(world_id);
        auto opaque_draw = wall;
        auto glass       = settings.glare && family == GlassFamily::legacy ? glare_pane : pane;
        auto layer       = second;
        transform(opaque_draw, settings.wall_x, settings.camera_x);
        transform(glass, settings.pane_x, settings.camera_x);
        transform(layer, 0, settings.camera_x);
        glass.texture_matrix[12] = settings.uv_shift;
        if (settings.zero_alpha)
        {
            const auto* zero = flatColor({ .13f, .4f, .6f, 0 });
            for (unsigned slot = 0; slot < glass.texture_count; ++slot)
                glass.diffuse_textures[slot] = zero;
        }
        if (settings.glare && glass.material_slot_count)
        {
            glass.materialSlots()[0].fullbright    = 0;
            glass.materialSlots()[0].env_intensity = 1.f;
            std::fill_n(glass.materialSlots()[0].specular_color, 4, 1.f);
        }
        std::vector<ALMetalWorldSimpleBatch> forwards;
        if (settings.overlap)
            forwards.push_back(layer);
        forwards.push_back(glass);
        auto g              = scene();
        g.width             = GW;
        g.height            = GH;
        g.hdr               = true;
        g.reverse_z         = true;
        g.batches           = &opaque_draw;
        g.batch_count       = 1;
        g.material_mip_bias = std::log2(float(GW) / GOW) - 1.f;
        g.source_owner      = resources;
        ALMetalWorldOpaquePostDeferredScene f;
        f.width              = GW;
        f.height             = GH;
        f.hdr                = true;
        f.reverse_z          = true;
        f.batches            = forwards.data();
        f.batch_count        = forwards.size();
        f.material_mip_bias  = g.material_mip_bias;
        f.alpha_environment  = &environment;
        f.alpha_lighting     = &lighting;
        f.legacy_environment = settings.glare ? bright_cube : neutral_cube;
        f.source_owner       = resources;
        ALMetalTemporalWorldHistory::FrameInput hi;
        hi.world_id           = world_id;
        hi.input_width        = GW;
        hi.input_height       = GH;
        hi.output_width       = GOW;
        hi.output_height      = GOH;
        hi.mode               = use_metalfx ? 6 : 8;
        hi.jitter_phase_count = GPhases;
        const auto p          = projection();
        std::copy(p.begin(), p.end(), hi.camera_mvp);
        hi.camera_mvp[12] = -p[0] * settings.camera_x;
        REQUIRE(history.beginFrame(hi, &error));
        REQUIRE(history.stage(g, f, &error));
        const auto  jitter = history.jitterPixels();
        GlassPixels result;
        result.jitter               = jitter;
        result.phase                = history.stats().jitter_phase;
        result.valid_previous       = history.stats().valid_previous_draws;
        g.temporal_jitter_pixels[0] = jitter[0];
        g.temporal_jitter_pixels[1] = jitter[1];
        ALMetalWorldTemporalPostConfig post;
        post.width = post.presentation_width = GOW;
        post.height = post.presentation_height = GOH;
        auto targets                           = renderer->prepareTemporalPostTargets(post, &error);
        REQUIRE(targets);
        REQUIRE(renderer->beginCommandBatch("Glass production color and Temporal input preparation", &error));
        auto gbuffer = renderer->render(g, &error);
        INFO(error);
        REQUIRE(gbuffer);
        REQUIRE(renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(), *opaque_depth, &error));
        REQUIRE(renderer->resolveTemporalColor(*gbuffer, *gbuffer, *hdr, &error));
        auto colors = forwards;
        for (auto& draw : colors)
            alMetalApplyTemporalJitter(draw.modelview_projection, jitter[0], jitter[1], GW, GH);
        auto descriptor                      = hdr->descriptor();
        descriptor.colors[0].texture         = hdr->colorTexture(0);
        descriptor.depth_stencil_format      = gbuffer->descriptor().depth_stencil_format;
        descriptor.depth_stencil_shader_read = true;
        descriptor.depth_stencil_texture     = gbuffer->depthStencilTexture();
        auto continuation                    = ALMetalRenderTarget::create(*device, descriptor, &error);
        REQUIRE(continuation);
        auto colored                  = f;
        colored.batches               = colors.data();
        colored.gbuffer               = gbuffer.get();
        colored.composed_lighting     = continuation.get();
        colored.reconstruction_opaque = opaque_color.get();
        auto final                    = renderer->renderOpaquePostDeferred(colored, std::move(continuation), &error);
        INFO(error);
        REQUIRE(final);
        ALMetalWorldTemporalMotionScene m;
        m.width                 = GW;
        m.height                = GH;
        m.reverse_z             = true;
        m.opaque_scene          = &g;
        m.forward_scene         = &f;
        m.depth                 = gbuffer->depthStencilTexture();
        m.opaque_depth          = opaque_depth->colorTexture(0);
        m.previous_opaque       = history.previousOpaque().data();
        m.previous_opaque_count = history.previousOpaque().size();
        auto previous_forward   = history.previousForward();
        if (settings.invalid_previous && !previous_forward.empty())
            previous_forward.back().valid = false;
        m.previous_forward       = previous_forward.data();
        m.previous_forward_count = previous_forward.size();
        m.reset                  = history.resetRequired();
        std::copy(jitter.begin(), jitter.end(), m.jitter_pixels);
        std::copy(history.clipToPrevious().begin(), history.clipToPrevious().end(), m.clip_to_previous_clip);
        simd_float4x4 matrix;
        std::memcpy(&matrix, p.data(), sizeof(matrix));
        const auto inverse = simd_inverse(matrix);
        std::memcpy(m.inverse_projection, &inverse, sizeof(inverse));
        auto motion = renderer->renderTemporalMotion(m, &error);
        INFO(error);
        REQUIRE(motion);
        // Preserve the real policy result before the sole test-only diagnostic.
        REQUIRE(renderer->commitCommandBatch(&error));
        auto diagnostic = device->makeCommandBuffer("Glass fixture preserve original rejection and optional localized diagnostic", &error);
        REQUIRE(diagnostic);
        REQUIRE(rejection_snapshot->copyFrom(*diagnostic, *motion->colorTexture(1), {}, &error));
        const bool stationary = has_previous_settings && settings.camera_x == previous_settings.camera_x &&
                                settings.pane_x == previous_settings.pane_x && settings.wall_x == previous_settings.wall_x &&
                                settings.uv_shift == previous_settings.uv_shift && settings.overlap == previous_settings.overlap &&
                                !settings.invalid_previous && !settings.glare && !settings.zero_alpha;
        if (diagnostic_composition_zero && stationary && !history.resetRequired())
        {
            std::vector<std::uint8_t> zero(48 * 40, 0);
            REQUIRE(const_cast<ALMetalTexture*>(motion->colorTexture(1))
                        ->upload(*diagnostic, 0, 0, { 20, 12, 48, 40 }, zero.data(), 48, &error));
        }
        REQUIRE(diagnostic->commitAndWait(&error)); // Fixture-only observation boundary, never a production wait.
        REQUIRE(renderer->beginMainFrameBatch(world_id, "Glass native reconstruction and final presentation", &error));
        ALMetalAuroraReconstructor::Ticket at = 0;
        ALMetalTemporalScaler::Ticket      mt = 0;
        if (use_metalfx)
        {
            ALMetalTemporalScalerFrame frame;
            frame.jitter_x = jitter[0];
            frame.jitter_y = jitter[1];
            frame.reset    = history.resetRequired();
            REQUIRE(renderer->encodeTemporalUpscale(*metalfx, { GW, GH, GOW, GOH }, *final->colorTexture(0), *motion, *reconstructed, frame,
                                                    mt, &error));
            REQUIRE(renderer->resolveTemporalColor(*final, *reconstructed, *resolved, &error, jitter.data()));
        }
        else
        {
            ALMetalAuroraFrame frame;
            alMetalAuroraSetViewerCamera(frame, p.data(), history.clipToPrevious().data());
            frame.jitter_x           = jitter[0];
            frame.jitter_y           = jitter[1];
            frame.jitter_phase_count = GPhases;
            frame.reset              = history.resetRequired();
            REQUIRE(renderer->encodeAuroraUpscale(*aurora, { GW, GH, GOW, GOH }, *final->colorTexture(0), *opaque_color->colorTexture(0),
                                                  *motion, *resolved, frame, at, &error));
        }
        ALMetalWorldDisplayPostScene display;
        display.source = resolved.get();
        display.hdr    = true;
        auto displayed = renderer->renderDisplayPost(display, &error, std::move(targets->display));
        REQUIRE(displayed);
        ALMetalWorldPresentationScene presentation;
        presentation.source        = displayed.get();
        presentation.output_width  = GOW;
        presentation.output_height = GOH;
        auto presented             = renderer->renderPresentationPost(presentation, &error, std::move(targets->presentation));
        REQUIRE(presented);
        const auto completed = renderer->commandBatchCompletion();
        REQUIRE(completed);
        REQUIRE(renderer->observeCommandBatchSettlement(
            [&](bool submitted)
            {
                if (submitted)
                {
                    if (use_metalfx)
                        REQUIRE(metalfx->submitted(mt, &error));
                    else
                        REQUIRE(aurora->submitted(at, &error));
                    REQUIRE(history.commit(world_id, &error));
                }
                else
                {
                    if (use_metalfx)
                        metalfx->abandon(mt);
                    else
                        aurora->abandon(at);
                    history.abandon();
                }
            },
            &error));
        REQUIRE(renderer->commitCommandBatch(&error));
        REQUIRE(completed->waitUntilCompleted(&error));
        INFO(error);
        result.color  = glassColors(*final->colorTexture(0), *device, error);
        result.opaque = glassColors(*opaque_color->colorTexture(0), *device, error);
        result.output = glassColors(*resolved->colorTexture(0), *device, error);
        result.actual_rejection.resize(GW * GH);
        result.used_rejection.resize(GW * GH);
        result.depth.resize(GW * GH);
        result.motion.resize(GW * GH);
        REQUIRE(rejection_snapshot->readback(*device, 0, 0, {}, result.actual_rejection.data(), GW, &error));
        REQUIRE(motion->colorTexture(1)->readback(*device, 0, 0, {}, result.used_rejection.data(), GW, &error));
        REQUIRE(motion->colorTexture(2)->readback(*device, 0, 0, {}, result.depth.data(), GW * 4, &error));
        REQUIRE(motion->colorTexture(0)->readback(*device, 0, 0, {}, result.motion.data(), GW * 4, &error));
        if (!use_metalfx)
        {
            result.generated_reactivity.resize(GW * GH);
            REQUIRE(aurora->generatedReactive()->readback(*device, 0, 0, {}, result.generated_reactivity.data(), GW, &error));
        }
        previous_settings     = settings;
        has_previous_settings = true;
        return result;
    }
};
double glassVariation(const std::vector<GColor>& a, const std::vector<GColor>& b, bool covered)
{
    double   sum   = 0;
    unsigned count = 0;
    for (unsigned y = 18; y < 78; ++y)
        for (unsigned x = covered ? 36 : 150; x < (covered ? 94 : 186); ++x)
            for (unsigned c = 0; c < 3; ++c)
            {
                sum += std::abs(a[y * GOW + x][c] - b[y * GOW + x][c]);
                ++count;
            }
    return sum / count;
}
double glassInputDifference(const std::vector<GColor>& a, const std::vector<GColor>& b)
{
    REQUIRE(a.size() == GW * GH);
    REQUIRE(b.size() == a.size());
    double difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        for (unsigned c = 0; c < 3; ++c)
            difference += std::abs(a[i][c] - b[i][c]);
    return difference / (a.size() * 3);
}
double glassEdgeEnergy(const std::vector<GColor>& a, bool covered)
{
    double   sum   = 0;
    unsigned count = 0;
    for (unsigned y = 18; y < 78; ++y)
        for (unsigned x = covered ? 36 : 150; x < (covered ? 94 : 186); ++x)
            for (unsigned c = 0; c < 3; ++c)
            {
                sum += std::abs(a[y * GOW + x][c] - a[y * GOW + x + 1][c]);
                ++count;
            }
    return sum / count;
}
double glassMaskMean(const std::vector<std::uint8_t>& mask, bool covered)
{
    double   sum   = 0;
    unsigned count = 0;
    for (unsigned y = 12; y < 52; ++y)
        for (unsigned x = covered ? 24 : 102; x < (covered ? 62 : 122); ++x)
        {
            sum += mask[y * GW + x] / 255.;
            ++count;
        }
    return sum / count;
}
struct GlassSequence
{
    std::vector<GlassPixels> frames;
    double                   variation = 0, uncovered_variation = 0, detail = 0, rejection = 0, generated_reactivity = 0;
};
GlassSequence glassSequence(GlassFamily family, bool diagnostic = false, bool metalfx = false)
{
    GlassFixture  fixture(family, metalfx);
    GlassSequence out;
    const auto    total = GWarm + GStable + 18 + 36;
    out.frames.reserve(total);
    for (unsigned frame = 0; frame < total; ++frame)
    {
        GlassSettings settings;
        if (frame >= GWarm + GStable)
        {
            const auto moving = std::min(frame - (GWarm + GStable) + 1, 18u);
            settings.camera_x = .0125f * moving;
        }
        auto pixels = fixture.render(settings, diagnostic);
        if (frame == 0)
        {
            double foreground_contribution = 0, background_detail = 0;
            for (unsigned y = 12; y < 52; ++y)
                for (unsigned x = 24; x < 62; ++x)
                    for (unsigned c = 0; c < 3; ++c)
                        foreground_contribution += std::abs(pixels.color[y * GW + x][c] - pixels.opaque[y * GW + x][c]);
            for (unsigned y = 12; y < 52; ++y)
                for (unsigned x = 102; x < 121; ++x)
                    for (unsigned c = 0; c < 3; ++c)
                        background_detail += std::abs(pixels.opaque[y * GW + x][c] - pixels.opaque[y * GW + x + 1][c]);
            INFO("Actual input foreground contribution=" << foreground_contribution
                                                         << " uncovered background detail=" << background_detail);
            REQUIRE(foreground_contribution > 1.f);
            REQUIRE(background_detail > 1.f); // A culled/flat fixture must never qualify a history policy.
        }
        if (frame >= GWarm && frame < GWarm + GStable)
        {
            REQUIRE(pixels.valid_previous == 2);
            out.variation += glassVariation(pixels.output, out.frames.back().output, true) / GStable;
            out.uncovered_variation += glassVariation(pixels.output, out.frames.back().output, false) / GStable;
            out.detail += glassEdgeEnergy(pixels.output, true) / GStable;
            out.rejection += glassMaskMean(pixels.actual_rejection, true) / GStable;
            if (!metalfx)
                out.generated_reactivity += glassMaskMean(pixels.generated_reactivity, true) / GStable;
        }
        if (!fixture.output_dir.empty())
        {
            auto directory = fixture.output_dir / (std::string(familyName(family)) + (metalfx      ? "-metalfx"
                                                                                      : diagnostic ? "-diagnostic"
                                                                                                   : "-production"));
            std::filesystem::create_directories(directory);
            std::ostringstream name;
            name << "world-" << std::setw(3) << std::setfill('0') << frame;
            glassRaw(directory / (name.str() + "-output.rgba32f"), pixels.output);
            if (frame >= GWarm && frame < GWarm + GPhases)
            {
                glassRaw(directory / (name.str() + "-color.rgba32f"), pixels.color);
                glassRaw(directory / (name.str() + "-opaque.rgba32f"), pixels.opaque);
                glassRaw(directory / (name.str() + "-rejection.r8"), pixels.actual_rejection);
                glassRaw(directory / (name.str() + "-generated.r8"), pixels.generated_reactivity);
                glassRaw(directory / (name.str() + "-depth.r32f"), pixels.depth);
                glassRaw(directory / (name.str() + "-motion.rg16f"), pixels.motion);
            }
        }
        out.frames.push_back(std::move(pixels));
    }
    if (!fixture.output_dir.empty())
    {
        const auto    name = std::string(familyName(family)) + (metalfx ? "-metalfx" : diagnostic ? "-diagnostic" : "-production");
        std::ofstream f(fixture.output_dir / (name + "-metrics.json"));
        f << std::setprecision(17) << "{\"family\":\"" << familyName(family) << "\",\"diagnostic\":" << (diagnostic ? "true" : "false")
          << ",\"metalfx\":" << (metalfx ? "true" : "false") << ",\"warm\":" << GWarm << ",\"stationary\":" << GStable
          << ",\"moving\":18,\"stopped\":36,\"phase_count\":18,\"variation\":" << out.variation
          << ",\"uncovered_variation\":" << out.uncovered_variation << ",\"detail\":" << out.detail << ",\"rejection\":" << out.rejection
          << ",\"generated_reactivity\":" << out.generated_reactivity << "}\n";
        REQUIRE(f.good());
    }
    return out;
}
void glassOverlapContract(const GlassPixels& actual, const GlassPixels& without, bool unsafe)
{
    CHECK(actual.phase == without.phase);
    CHECK(actual.jitter == without.jitter);
    unsigned contributing = 0, uncovered = 0, wrong_contributing = 0, wrong_uncovered = 0, wrong_depth = 0;
    for (unsigned y = 12; y < 52; ++y)
        for (unsigned x = 24; x < 62; ++x)
        {
            const auto i          = y * GW + x;
            float      difference = 0;
            for (unsigned c = 0; c < 3; ++c)
                difference += std::abs(actual.color[i][c] - without.color[i][c]);
            if (difference > 1.e-5f)
            {
                ++contributing;
                wrong_contributing += actual.actual_rejection[i] != (unsafe ? 255 : 0);
            }
            else
            {
                ++uncovered;
                wrong_uncovered += actual.actual_rejection[i] != without.actual_rejection[i];
            }
            wrong_depth += std::abs(actual.depth[i] - without.depth[i]) > 1.e-6f;
        }
    INFO("Actually contributing overlap pixels=" << contributing << " unchanged pixels=" << uncovered);
    REQUIRE(contributing > 100);
    REQUIRE(uncovered > 100);
    CHECK(wrong_contributing == 0);
    CHECK(wrong_uncovered == 0);
    CHECK(wrong_depth == 0);
}
} // namespace

TEST_CASE("Static tinted panes retain coherent history through actual Aurora reconstruction",
          "[metal][temporal-glass][temporal-glass-stability]")
{
    for (auto family : { GlassFamily::legacy, GlassFamily::pbr_single, GlassFamily::pbr_multi })
    {
        CAPTURE(familyName(family));
        const auto actual = glassSequence(family), diagnostic = glassSequence(family, true);
        INFO("stationary covered=" << actual.variation << " uncovered=" << actual.uncovered_variation
                                   << " diagnostic=" << diagnostic.variation << " detail=" << actual.detail
                                   << " rejection=" << actual.rejection << " generated=" << actual.generated_reactivity);
        CHECK(actual.rejection == 0.0);       // A static coherent partial layer is not an unknown/dynamic composition.
        CHECK(actual.variation <= 1. / 255.); // Existing stationary mean-RGB stability bound, unchanged.
        double difference = 0, input_difference = 0;
        for (unsigned i = GWarm; i < GWarm + GStable; ++i)
        {
            difference += glassVariation(actual.frames[i].output, diagnostic.frames[i].output, true) / GStable;
            input_difference += glassInputDifference(actual.frames[i].color, diagnostic.frames[i].color) / GStable;
        }
        CHECK(input_difference == 0);
        CHECK(difference < .001); // Sole diagnostic differs only in covered composition permission.
        CHECK(actual.detail >= diagnostic.detail * .99);
    }
}

TEST_CASE("Glass rejection protects genuinely different or invalid contributing history", "[metal][temporal-glass][temporal-glass-inputs]")
{
    for (auto family : { GlassFamily::legacy, GlassFamily::pbr_single, GlassFamily::pbr_multi })
    {
        CAPTURE(familyName(family));
        GlassFixture f(family), control(family);
        f.render();
        control.render();
        auto stable = f.render();
        control.render();
        CHECK(glassMaskMean(stable.actual_rejection, true) == 0);
        GlassSettings camera;
        camera.camera_x = .1f;
        auto parallax   = f.render(camera);
        control.render(camera);
        CHECK(glassMaskMean(parallax.actual_rejection, true) > 0.9);
        auto stopped = f.render(camera);
        control.render(camera);
        CHECK(glassMaskMean(stopped.actual_rejection, true) == 0);
        GlassSettings foreground = camera;
        foreground.pane_x        = .1f;
        auto moving              = f.render(foreground);
        control.render(foreground);
        CHECK(glassMaskMean(moving.actual_rejection, true) > 0.9);
        GlassSettings background = foreground;
        background.wall_x        = .1f;
        auto moving_background   = f.render(background);
        control.render(background);
        CHECK(glassMaskMean(moving_background.actual_rejection, true) > 0.9);
        GlassSettings animation = background;
        animation.uv_shift      = .1f;
        auto animated           = f.render(animation);
        control.render(animation);
        CHECK(glassMaskMean(animated.actual_rejection, true) > 0.9);
        GlassSettings invalid    = animation;
        invalid.invalid_previous = true;
        auto untrusted           = f.render(invalid);
        control.render(invalid);
        CHECK(glassMaskMean(untrusted.actual_rejection, true) > 0.9);
        GlassSettings layered    = invalid;
        layered.invalid_previous = false;
        layered.overlap          = true;
        auto overlap             = f.render(layered);
        auto without_settings    = layered;
        without_settings.overlap = false;
        auto without             = control.render(without_settings);
        glassOverlapContract(overlap, without, true); // Unsafe only where the new layer actually contributes RGB.
        const auto settled_overlap = f.render(layered), settled_without = control.render(without_settings);
        glassOverlapContract(settled_overlap, settled_without, false);
        ++f.second.temporal_generation;
        const auto replaced = f.render(layered), replacement_without = control.render(without_settings);
        glassOverlapContract(replaced, replacement_without, true);
        f.second.diffuse_textures[0] = f.flatColor({ .65f, .12f, .1f, 0.f });
        const auto zero_layer = f.render(layered), zero_without = control.render(without_settings);
        CHECK(glassInputDifference(zero_layer.color, zero_without.color) == 0);
        CHECK(zero_layer.actual_rejection == zero_without.actual_rejection);
        const auto resets_before_content_update = f.history.stats().resets;
        ++f.pane.temporal_material_revision; // Same native allocation/geometry, newly published image contents.
        const auto updated_content = f.render(layered);
        CHECK(glassMaskMean(updated_content.actual_rejection, true) > .9);
        const auto stable_content = f.render(layered);
        CHECK(glassMaskMean(stable_content.actual_rejection, true) == 0);
        CHECK(f.history.stats().resets == resets_before_content_update); // Local material invalidation, not a camera/history reset.
    }
}

TEST_CASE("MetalFX consumes the shared static glass history policy", "[metal][temporal-glass][temporal-glass-metalfx]")
{
    const auto actual = glassSequence(GlassFamily::legacy, false, true);
    CHECK(actual.rejection == 0.0);
    INFO("MetalFX covered variation=" << actual.variation << " uncovered=" << actual.uncovered_variation << " detail=" << actual.detail);
}

TEST_CASE("Zero contribution preserves background while legacy glare is still visible", "[metal][temporal-glass][temporal-glass-alpha]")
{
    for (auto family : { GlassFamily::legacy, GlassFamily::pbr_single, GlassFamily::pbr_multi })
    {
        GlassFixture  f(family);
        GlassSettings zero;
        zero.zero_alpha = true;
        f.render(zero);
        const auto p = f.render(zero);
        CAPTURE(familyName(family));
        double contribution = 0;
        for (unsigned y = 12; y < 52; ++y)
            for (unsigned x = 24; x < 62; ++x)
                for (unsigned c = 0; c < 3; ++c)
                    contribution += std::abs(p.color[y * GW + x][c] - p.opaque[y * GW + x][c]);
        CHECK(contribution < .001);
        CHECK(glassMaskMean(p.actual_rejection, true) == 0);
        CHECK(std::abs(p.depth[32 * GW + 32] - .025f) < 1.e-6f);
    }
    GlassFixture  f(GlassFamily::legacy);
    GlassSettings glare;
    glare.glare = true;
    f.render(glare);
    const auto p            = f.render(glare);
    double     contribution = 0;
    for (unsigned y = 12; y < 52; ++y)
        for (unsigned x = 24; x < 62; ++x)
            for (unsigned c = 0; c < 3; ++c)
                contribution += std::abs(p.color[y * GW + x][c] - p.opaque[y * GW + x][c]);
    INFO("Legacy zero-diffuse-alpha half-vertex-alpha glare RGB contribution=" << contribution);
    CHECK(contribution > .1);
    CHECK(glassMaskMean(p.actual_rejection, true) == 0);
    CHECK(std::abs(p.depth[32 * GW + 32] - .05f) < 1.e-6f); // Actual glare alpha supplies visible foreground despite raw alpha0.
    glare.pane_x      = .1f;
    const auto moving = f.render(glare);
    CHECK(glassMaskMean(moving.actual_rejection, true) > .9); // Differently moving background still contributes through glare.
}

TEST_CASE("Glass depth motion and rejection agree with actual jittered color coverage", "[metal][temporal-glass][temporal-glass-boundary]")
{
    for (auto family : { GlassFamily::legacy, GlassFamily::pbr_single, GlassFamily::pbr_multi })
    {
        CAPTURE(familyName(family));
        GlassFixture f(family);
        f.render();
        for (unsigned phase = 0; phase < GPhases; ++phase)
        {
            const auto p      = f.render();
            unsigned   inside = 0, outside = 0, wrong_depth = 0, wrong_motion = 0, wrong_rejection = 0, wrong_color = 0;
            // Exclude1.25 input pixels around the raster edge, leaving every
            // confidently interior/exterior texel tied to the actual jitter.
            const float left = .1f * GW + p.jitter[0], right = .6f * GW + p.jitter[0], top = .1f * GH + p.jitter[1],
                        bottom = .9f * GH + p.jitter[1];
            for (unsigned y = 4; y < GH - 4; ++y)
                for (unsigned x = 4; x < GW - 4; ++x)
                {
                    const float px = x + .5f, py = y + .5f;
                    const bool  in  = px > left + 1.25f && px < right - 1.25f && py > top + 1.25f && py < bottom - 1.25f;
                    const bool  out = px < left - 1.25f || px > right + 1.25f || py < top - 1.25f || py > bottom + 1.25f;
                    if (!in && !out)
                        continue;
                    const auto i          = y * GW + x;
                    float      difference = 0;
                    for (unsigned c = 0; c < 3; ++c)
                        difference += std::abs(p.color[i][c] - p.opaque[i][c]);
                    if (in)
                    {
                        ++inside;
                        wrong_color += difference < 1.e-5f;
                    }
                    else
                    {
                        ++outside;
                        wrong_color += difference > 1.e-5f;
                    }
                    wrong_depth += std::abs(p.depth[i] - (in ? .05f : .025f)) > 1.e-6f;
                    wrong_motion += std::abs(float(p.motion[i][0])) > 1.e-5f || std::abs(float(p.motion[i][1])) > 1.e-5f;
                    wrong_rejection += p.actual_rejection[i] != 0;
                }
            CAPTURE(phase, p.jitter[0], p.jitter[1], inside, outside);
            REQUIRE(inside > 100);
            REQUIRE(outside > 100);
            CHECK(wrong_color == 0);
            CHECK(wrong_depth == 0);
            CHECK(wrong_motion == 0);
            CHECK(wrong_rejection == 0);
        }
    }
}

TEST_CASE("Multi-slot PBR glass clamps invalid selectors exactly like its color pass", "[metal][temporal-glass][temporal-glass-index]")
{
    GlassFixture selected(GlassFamily::pbr_multi), overflow(GlassFamily::pbr_multi);
    const auto   configure = [](GlassFixture& f, unsigned selector)
    {
        GlassFixture::Geometry g;
        REQUIRE(f.pane.vertices->read(0, &g, sizeof(g), &f.error));
        for (unsigned i = 4; i < 8; ++i)
        {
            g.positions[i][3] = std::bit_cast<float>(std::int32_t(selector));
            g.colors[i][3]    = 102;
        }
        f.pane.vertices            = f.buffer(&g, sizeof(g));
        f.pane.diffuse_textures[0] = f.flatColor({ .13f, .4f, .6f, 0.f });
        f.pane.diffuse_textures[1] = f.patternedPane(.45f);
    };
    configure(selected, 1);
    configure(overflow, 5);
    selected.render();
    overflow.render();
    for (unsigned frame = 0; frame < 2; ++frame)
    {
        GlassSettings settings;
        settings.camera_x   = frame ? .1f : 0.f;
        const auto expected = selected.render(settings), actual = overflow.render(settings);
        CHECK(expected.jitter == actual.jitter);
        CHECK(glassInputDifference(actual.color, expected.color) == 0);
        CHECK(actual.actual_rejection == expected.actual_rejection);
        CHECK(actual.depth == expected.depth);
        CHECK(actual.motion == expected.motion);
        const auto visible = 32 * GW + 56, zero = 32 * GW + 30;
        float      color_contribution = 0;
        for (unsigned c = 0; c < 3; ++c)
            color_contribution += std::abs(actual.color[visible][c] - actual.opaque[visible][c]);
        REQUIRE(color_contribution > .001f);
        CHECK(std::abs(actual.depth[visible] - .05f) < 1.e-6f);
        CHECK(std::abs(actual.depth[zero] - .025f) < 1.e-6f);
        CHECK(actual.actual_rejection[visible] == (frame ? 255 : 0));
        if (!overflow.output_dir.empty())
        {
            const auto dir = overflow.output_dir / "pbr-index-5-vs-1";
            std::filesystem::create_directories(dir);
            const auto prefix = std::string("frame-") + std::to_string(frame);
            glassRaw(dir / (prefix + "-actual-color.rgba32f"), actual.color);
            glassRaw(dir / (prefix + "-expected-color.rgba32f"), expected.color);
            glassRaw(dir / (prefix + "-actual-rejection.r8"), actual.actual_rejection);
            glassRaw(dir / (prefix + "-expected-rejection.r8"), expected.actual_rejection);
            glassRaw(dir / (prefix + "-actual-depth.r32f"), actual.depth);
            glassRaw(dir / (prefix + "-expected-depth.r32f"), expected.depth);
        }
    }
}
