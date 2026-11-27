/** Jittered water reconstruction through the production field, haze and Aurora owners.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalauroraconventions.h"
#include "../almetaltemporalworldhistory.h"
#include "../../alrender/alwaterfield.h"
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
void waterProductionJitter(ALMetalWorldWaterHaze& field, const std::array<float, 2>& jitter,
    unsigned width = WW, unsigned height = WH)
{
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
    alMetalApplyTemporalWaterJitter(field, jitter[0], jitter[1], width, height);
#else
    alMetalApplyTemporalJitter(field.modelview_projection, jitter[0], jitter[1], width, height);
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
std::vector<WColor> waterReadDisplay(const ALMetalTexture& texture, ALMetalDevice& device, std::string& error)
{
    const auto format=texture.descriptor().format;
    if(format==ALMetalPixelFormat::rgba16_float)return waterRead(texture,device,error);
    std::vector<WColor> result(std::size_t(texture.width())*texture.height());
    if(format==ALMetalPixelFormat::rgb10a2_unorm)
    {
        std::vector<std::uint32_t> packed(result.size());
        REQUIRE(texture.readback(device,0,0,{},packed.data(),texture.width()*4,&error));
        for(unsigned i=0;i<packed.size();++i)
            result[i]={float(packed[i]&1023u)/1023.f,float((packed[i]>>10)&1023u)/1023.f,
                float((packed[i]>>20)&1023u)/1023.f,float(packed[i]>>30)/3.f};
    }
    else
    {
        REQUIRE((format==ALMetalPixelFormat::rgba8_unorm || format==ALMetalPixelFormat::bgra8_unorm));
        std::vector<std::array<std::uint8_t,4>> bytes(result.size());
        REQUIRE(texture.readback(device,0,0,{},bytes.data(),texture.width()*4,&error));
        for(unsigned i=0;i<bytes.size();++i)
        {
            const unsigned red=format==ALMetalPixelFormat::bgra8_unorm?2:0,blue=2-red;
            result[i]={bytes[i][red]/255.f,bytes[i][1]/255.f,bytes[i][blue]/255.f,bytes[i][3]/255.f};
        }
    }
    return result;
}
struct WaterPixels
{
    std::vector<WColor> raw, opaque, output;
    std::vector<WColor> immediate, presented, internal_history, luma_history;
    std::vector<float> depth, color_depth, captured_water_depth;
    std::vector<float> luma_instability;
    std::vector<std::array<__fp16, 2>> motion, dilated_motion;
    std::vector<std::uint8_t> unsafe, reactive, water_coverage;
    std::vector<std::uint8_t> shading_change, accumulation, previous_accumulation;
    std::vector<std::array<std::uint8_t,4>> masks;
    std::array<float, 2> jitter{};
    std::array<float, 2> aurora_previous_jitter{};
    unsigned phase = 0, field_draws = 0;
    bool pane_previous_valid = false;
    bool aurora_reset = true;
    float aurora_frame_index = 0.f;
    std::uint64_t aurora_ticket = 0;
    unsigned aurora_write_parity = 0, aurora_read_parity = 0;
    double presented_same_row_max_error=0,presented_flipped_row_mae=0;
    std::uint64_t ssr_generation_used=0,ssr_source_world_used=0,ssr_generation_after=0,ssr_source_world_after=0;
    bool ssr_history_published=false,ssr_history_skipped=false;
    std::vector<std::uint32_t> ssr_source_color_packed;
    std::vector<float> ssr_source_depth;
    std::vector<std::uint8_t> ssr_water_coverage_used;
    ALMetalWorldScreenSpaceReflectionCoordinates ssr_used_coordinates;
};
template<unsigned InputWidth = WW, unsigned InputHeight = WH,
    unsigned OutputWidth = WOW, unsigned OutputHeight = WOH>
struct WaterFixture : Fixture
{
    static constexpr unsigned WW = InputWidth, WH = InputHeight;
    static constexpr unsigned WOW = OutputWidth, WOH = OutputHeight;
    enum class SceneConfiguration { original, grazing_reflector };
    SceneConfiguration configuration = SceneConfiguration::original;
    ALMetalTemporalWorldHistory history;
    std::unique_ptr<ALMetalAuroraReconstructor> aurora;
    std::unique_ptr<ALMetalRenderTarget> hdr, opaque_color, opaque_depth, output;
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
    std::unique_ptr<ALMetalRenderTarget> water_inputs;
#endif
    ALMetalWorldAlphaEnvironment environment;
    ALMetalWorldLegacyAlphaLighting lighting;
    ALMetalWorldReflectionProbes ssr_probes;
    ALMetalWorldScreenSpaceReflectionHistory ssr_history;
    float authored_fresnel_offset = .1f; // Existing arms retain their original water material.
    ALMetalWorldSimpleBatch ssr_reflector;
    bool ssr_reflector_in_main=false,advance_ssr_history=false,skip_ssr_publication=false;
    std::vector<std::uint8_t> last_published_water_coverage; // Fixture provenance only, never an algorithm input.
    const ALMetalTexture *cells = nullptr, *empty_cells = nullptr, *normal = nullptr, *legacy_cube = nullptr;
    ALMetalWorldSimpleBatch floor, foreground;
    std::uint64_t world = 0;
    std::filesystem::path directory;

    bool grazingReflector() const noexcept
    {
        return configuration == SceneConfiguration::grazing_reflector;
    }
    WMatrix projection() const
    {
        auto result = waterProjection(WW, WH);
        if (grazingReflector())
        {
            constexpr float near = .1f, far = 512.f;
            result[10] = near / (far - near);
            result[14] = far * near / (far - near);
        }
        return result;
    }
    WMatrix view(float camera_x) const
    {
        auto result = waterView(camera_x);
        if (grazingReflector())
        {
            // The original view scaled tenfold: authored camera at (x,-30,20).
            result[13] = 2.f;
            result[14] = -36.f;
        }
        return result;
    }

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
        f.cell_square[0] = f.cell_square[1] = grazingReflector() ? -200.f : -4.f;
        f.cell_square[2] = grazingReflector() ? 400.f : 8.f; f.cell_square[3] = 1.f;
        f.rings[0] = 4.f; f.rings[1] = 1.f; f.rings[2] = 4.f;
        std::copy_n(environment.water_plane, 4, f.water_plane);
        f.fog_color[0] = .1f; f.fog_color[1] = .35f; f.fog_color[2] = .65f;
        f.fog[0] = 1.f; f.fog[1] = .25f;
        f.draws[0] = {0,4,1,true}; f.draws[1] = {1,9,12,false}; f.draws[2] = {2,9,4,false}; f.draw_count = 3;
        std::copy(v.begin(), v.end(), w.modelview);
        for (unsigned column = 0; column < 3; ++column)
            for (unsigned row = 0; row < 3; ++row) w.normal_matrix[column][row] = v[column*4+row];
        w.eye_position[0] = camera_x;
        w.eye_position[1] = grazingReflector() ? -30.f : -3.f;
        w.eye_position[2] = grazingReflector() ? 20.f : 2.f;
        w.normal_maps = { normal, normal }; w.normal_maps_mipped = true;
        w.water_far[0] = 10000.f;
        w.normal_scale_glint[0] = w.normal_scale_glint[1] = w.normal_scale_glint[2] = 2.f;
        w.normal_scale_glint[3] = .35f;
        w.optics[0] = .2f; w.optics[1] = authored_fresnel_offset;
        w.optics[2] = grazingReflector() ? .08f : .25f;
        w.parameters[1] = .1f; w.parameters[2] = 1.f;
        w.parameters[3] = grazingReflector() ? 1.f : 0.f;
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
    void publishWaterSsrSource()
    {
        // A real source draw with the same perspective camera as the water.
        // Author the history plane in view space, then transform its positions
        // to world space; no invented camera/projection or uploaded fake depth.
        const auto p=projection(),v=view(0.f);
        const auto iv=simd_inverse(waterSimd(v.data()));
        const auto pv=waterMatrix(simd_mul(waterSimd(p.data()),waterSimd(v.data())));
        const auto image=[&](ALMetalTextureDescriptor d,const void* bytes,unsigned row)
        {
            d.usage=ALMetalTextureUsage::shader_read;
            std::shared_ptr<ALMetalTexture> value=ALMetalTexture::create(*device,d,&error);REQUIRE(value);
            auto upload=device->makeCommandBuffer("Water SSR authored texture",&error);REQUIRE(upload);
            const unsigned faces=d.type==ALMetalTextureType::texture_cube_array?6:1;
            for(unsigned face=0;face<faces;++face)REQUIRE(value->upload(*upload,0,face,{},bytes,row,&error));
            REQUIRE(upload->commitAndWait(&error));resources->textures.push_back(value);return value.get();
        };
        ALMetalTextureDescriptor d;d.width=d.height=d.array_length=1;
        d.type=ALMetalTextureType::texture_cube_array;d.format=ALMetalPixelFormat::rgba16_float;
        const WHalf blue{__fp16(.125f),__fp16(.375f),__fp16(.75f),__fp16(1.f)};
        ssr_probes.radiance_texture=image(d,blue.data(),8);
        d.type=ALMetalTextureType::texture_2d;d.width=4;d.height=1;d.format=ALMetalPixelFormat::rgba32_uint;
        std::array<std::uint32_t,16> coefficients{};std::array<std::uint16_t,4> packed;
        std::memcpy(packed.data(),blue.data(),sizeof(blue));
        coefficients[0]=packed[0]|(std::uint32_t(packed[1])<<16);coefficients[1]=packed[2];
        ssr_probes.irradiance_texture=image(d,coefficients.data(),64);
        ssr_probes.constants.config[0]=1.f;ssr_probes.constants.ref_sphere[0][3]=100.f;
        ssr_probes.constants.ref_params[0][0]=ssr_probes.constants.ref_params[0][1]=ssr_probes.constants.ref_params[0][2]=1.f;
        ssr_probes.constants.ref_params[0][3]=-100.f;ssr_probes.constants.ref_index[0][1]=-1;
        std::vector<WHalf> pattern(WW*WH);
        for(unsigned y=0;y<WH;++y)for(unsigned x=0;x<WW;++x)
        {
            const float u=(x+.5f)/WW,w=(y+.5f)/WH;
            const float stripe=std::max(0.f,1.f-std::abs(u-.36f)/.055f);
            const float cross=std::max(0.f,1.f-std::abs(w-.73f)/.08f)*(u>.55f&&u<.8f);
            pattern[y*WW+x]={__fp16(.05f+4.f*stripe),__fp16(.04f+2.f*cross),__fp16(.02f),__fp16(1.f)};
        }
        d.width=WW;d.height=WH;d.format=ALMetalPixelFormat::rgba16_float;
        const auto* detail=image(d,pattern.data(),WW*8);
        Geometry geometry{};const float xy[4][2]{{-1,-1},{1,-1},{1,1},{-1,1}};
        constexpr float distance=4.f;
        for(unsigned i=0;i<4;++i)
        {
            const auto q=simd_mul(iv,simd_make_float4(xy[i][0]*distance/p[0],xy[i][1]*distance/p[5],-distance,1.f));
            std::memcpy(geometry.positions[i],&q,sizeof(q));geometry.normals[i][2]=1.f;
            geometry.uv[i][0]=i==1||i==2;geometry.uv[i][1]=i>=2;std::fill_n(geometry.colors[i],4,255);
        }
        const std::uint16_t indices[6]{0,1,2,0,2,3};auto draw=batch(0);
        draw.vertices=buffer(&geometry,sizeof(geometry));draw.indices=buffer(indices,sizeof(indices));
        draw.vertex_count=4;draw.index_count=6;draw.texture_count=1;draw.diffuse_textures[0]=detail;draw.impostor_fullbright=true;
        std::copy(v.begin(),v.end(),draw.modelview);std::copy(pv.begin(),pv.end(),draw.modelview_projection);identity(draw.texture_matrix);
        draw.temporal_instance_id=704;draw.temporal_generation=1;ssr_reflector=draw;
        data->beginFrame(++world);auto lit=renderer->prepareTemporalTarget(WW,WH,false,&error);REQUIRE(lit);
        REQUIRE(renderer->beginCommandBatch("Water SSR genuine bright source",&error));
        auto empty=scene();empty.width=WW;empty.height=WH;empty.hdr=empty.normal_hdr=empty.reverse_z=true;
        auto gbuffer=renderer->render(empty,&error);REQUIRE(gbuffer);REQUIRE(renderer->resolveTemporalColor(*gbuffer,*gbuffer,*lit,&error));
        ALMetalWorldOpaquePostDeferredScene forward;forward.gbuffer=gbuffer.get();forward.composed_lighting=lit.get();
        forward.batches=&draw;forward.batch_count=1;forward.width=WW;forward.height=WH;forward.hdr=forward.reverse_z=true;
        forward.publish_screen_space_reflection_history=true;
        auto source=renderer->renderOpaquePostDeferred(forward,&error);INFO(error);REQUIRE(source);
        auto completion=renderer->commandBatchCompletion();REQUIRE(completion);REQUIRE(renderer->commitCommandBatch(&error));
        REQUIRE(completion->waitUntilCompleted(&error));
        ALMetalWorldScreenSpaceReflectionCoordinates coordinates;
        std::copy(p.begin(),p.end(),coordinates.projection);
        const auto ip=waterMatrix(simd_inverse(waterSimd(p.data())));std::copy(ip.begin(),ip.end(),coordinates.inverse_projection);
        std::copy(v.begin(),v.end(),coordinates.modelview);coordinates.reverse_z=true;
        coordinates.camera_generation=coordinates.source_world=world;
        REQUIRE(renderer->updateScreenSpaceReflectionHistory(*source,true,coordinates,ssr_history,&error));
        REQUIRE(ssr_history.valid());
        // Verify actual rasterized/published signal before tracing: correct
        // perspective source depth and a genuinely bright authored reflector.
        std::vector<float> published_depth(WW*WH);
        REQUIRE(ssr_history.target()->colorTexture(1)->readback(*device,0,0,{},published_depth.data(),WW*4,&error));
        const float expected_depth=(-distance*p[10]+p[14])/distance;
        float depth_error=0;for(float value:published_depth)depth_error=std::max(depth_error,std::abs(value-expected_depth));
        REQUIRE(depth_error<1.e-6f);
        const auto source_pixels=waterRead(*source->colorTexture(0),*device,error);
        float brightest=0;for(const auto& value:source_pixels)brightest=std::max(brightest,value[0]);
        REQUIRE(brightest>2.f);
        last_published_water_coverage.assign(WW*WH,0); // Seed is only the genuine reflector, not water.
    }
    ALMetalWorldScreenSpaceReflections waterSsr(float camera_x,unsigned budget)
    {
        ALMetalWorldScreenSpaceReflections ssr;
        ssr.constants.trace[0]=25.f;ssr.constants.trace[1]=.1f;ssr.constants.trace[2]=.04f;ssr.constants.trace[3]=.001f;
        ssr.constants.sampling[0]=float(std::max(1u,budget));ssr.constants.sampling[1]=1.6f;
        const auto iv=waterMatrix(simd_inverse(waterSimd(view(camera_x).data())));const double origin[3]{};
        REQUIRE(ssr_history.bind(ssr,iv.data(),origin,&error));
        return ssr;
    }
    WaterPixels render(bool independent_inverse = false, float camera_x = 0.f, float animation = 0.f,
        bool include_water = true, bool drawn_cell = true, bool use_surface_inputs = true,
        const ALMetalWorldSimpleBatch* transparent_pane = nullptr, bool invalid_pane_previous = false,
        bool capture_diagnostics = false, const ALMetalWorldScreenSpaceReflections* screen_space_reflections = nullptr)
    {
        ++world; data->beginFrame(world);
        const auto p = projection(), v = view(camera_x);
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
        if (grazingReflector())
        {
            const auto phases = ALWaterField::wavePhases(camera_x, -30., double(animation),
                LLVector2(.4f, .7f), LLVector2(-.3f, .5f));
            std::copy_n(phases.mBig.mV, 2, surface.big_phase);
            std::copy_n(phases.mLittle.mV, 4, surface.little_phase);
            std::copy_n(phases.mSweep.mV, 2, surface.sweep_phase);
            const auto lattice = ALWaterField::latticePlaces(camera_x, -30.);
            std::copy_n(lattice.mBase, 4, surface.lattice_base);
            std::copy_n(lattice.mFrac.mV, 4, surface.lattice_fraction);
        }
        else
        {
            surface.big_phase[0] = animation;
            surface.little_phase[1] = animation * .7f;
        }
        std::array<ALMetalWorldSimpleBatch, 4> markers{};
        unsigned forward_count=0;
        if(ssr_reflector_in_main)
        {
            REQUIRE(ssr_history.valid());
            auto& reflector=markers[forward_count++];reflector=ssr_reflector;
            std::copy(v.begin(),v.end(),reflector.modelview);std::copy(camera.begin(),camera.end(),reflector.modelview_projection);
        }
        // Match the production above-water refraction ordering: fog what is
        // behind the field before copying it for the surface's refraction.
        if(include_water){markers[forward_count++].water_haze=true;markers[forward_count++].water_surface=true;}
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
        f.alpha_environment = &environment; f.alpha_lighting = &lighting;
        // Preparation selects the probe context independently of SSR. The
        // no-SSR control passes null while preserving this identical context.
        f.legacy_environment = ssr_history.valid() ? nullptr : legacy_cube;
        f.reflection_probes = ssr_history.valid() ? &ssr_probes : nullptr;
        f.screen_space_reflections = screen_space_reflections;
        f.publish_screen_space_reflection_history=advance_ssr_history;
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
        f.temporal_water_inputs = use_surface_inputs ? water_inputs.get() : nullptr;
#endif
        ALMetalTemporalWorldHistory::FrameInput h;
        h.world_id = world; h.input_width = WW; h.input_height = WH; h.output_width = WOW; h.output_height = WOH;
        h.mode = 8; h.reverse_z = true; h.jitter_phase_count = WP;
        std::copy(camera.begin(), camera.end(), h.camera_mvp);
        REQUIRE(history.beginFrame(h, &error)); REQUIRE(history.stage(g, f, &error));
        WaterPixels pixels; pixels.jitter = history.jitterPixels(); pixels.phase = history.stats().jitter_phase;
        if(advance_ssr_history)
        {
            REQUIRE(ssr_history.valid());REQUIRE(ssr_history.coordinates());
            pixels.ssr_generation_used=ssr_history.generation();pixels.ssr_source_world_used=ssr_history.coordinates()->source_world;
            pixels.ssr_used_coordinates=*ssr_history.coordinates();
            if(screen_space_reflections)
            {
                REQUIRE(screen_space_reflections->scene_color==ssr_history.target()->colorTexture(0));
                REQUIRE(screen_space_reflections->scene_depth==ssr_history.target()->colorTexture(1));
            }
        }
        if (transparent_pane && !history.previousForward().empty())
            pixels.pane_previous_valid = history.previousForward().back().valid && !invalid_pane_previous;
        std::copy(pixels.jitter.begin(), pixels.jitter.end(), g.temporal_jitter_pixels);
        waterProductionJitter(surface.field, pixels.jitter, WW, WH);
        waterProductionJitter(haze, pixels.jitter, WW, WH);
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
        if (transparent_pane || ssr_reflector_in_main)
        {
            // History/motion retain authoritative unjittered poses. Color
            // uses exactly the opaque/water producer's current raster jitter.
            if(ssr_reflector_in_main)alMetalApplyTemporalJitter(color_batches[0].modelview_projection,
                pixels.jitter[0],pixels.jitter[1],WW,WH);
            if(transparent_pane)alMetalApplyTemporalJitter(color_batches[forward_count-1].modelview_projection,
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
#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
        if (capture_diagnostics)
        {
            const auto snapshot = aurora->diagnostics();
            REQUIRE(snapshot.submitted); REQUIRE(snapshot.ticket==ticket); REQUIRE(snapshot.completion);
            REQUIRE(snapshot.completion->status()==ALMetalCommandStatus::completed);
            REQUIRE(snapshot.descriptor.input_width==WW); REQUIRE(snapshot.descriptor.input_height==WH);
            REQUIRE(snapshot.descriptor.output_width==WOW); REQUIRE(snapshot.descriptor.output_height==WOH);
            REQUIRE(snapshot.generated_reactive); REQUIRE(snapshot.shading_change); REQUIRE(snapshot.masks);
            REQUIRE(snapshot.dilated_motion);
            REQUIRE(snapshot.accumulation); REQUIRE(snapshot.previous_accumulation); REQUIRE(snapshot.luma_instability);
            REQUIRE(snapshot.luma_history); REQUIRE(snapshot.internal_history); REQUIRE(snapshot.immediate_output);
            pixels.immediate = waterRead(*snapshot.immediate_output, *device, error);
            pixels.internal_history = waterRead(*snapshot.internal_history, *device, error);
            pixels.luma_history = waterRead(*snapshot.luma_history, *device, error);
            pixels.shading_change.resize((WW/2)*(WH/2)); pixels.masks.resize(WW*WH);
            pixels.accumulation.resize(WW*WH); pixels.previous_accumulation.resize(WW*WH);
            pixels.dilated_motion.resize(WW*WH);
            pixels.luma_instability.resize(WW*WH);
            std::vector<__fp16> instability(WW*WH);
            REQUIRE(snapshot.shading_change->readback(*device,0,0,{},pixels.shading_change.data(),WW/2,&error));
            REQUIRE(snapshot.masks->readback(*device,0,0,{},pixels.masks.data(),WW*4,&error));
            REQUIRE(snapshot.accumulation->readback(*device,0,0,{},pixels.accumulation.data(),WW,&error));
            REQUIRE(snapshot.previous_accumulation->readback(*device,0,0,{},pixels.previous_accumulation.data(),WW,&error));
            REQUIRE(snapshot.dilated_motion->readback(*device,0,0,{},pixels.dilated_motion.data(),WW*4,&error));
            REQUIRE(snapshot.luma_instability->readback(*device,0,0,{},instability.data(),WW*2,&error));
            std::copy(instability.begin(),instability.end(),pixels.luma_instability.begin());
            REQUIRE(presented->colorTexture(0)->descriptor().format==ALMetalPixelFormat::bgra8_unorm);
            std::vector<std::array<std::uint8_t,4>> bgra(WOW*WOH);
            REQUIRE(presented->colorTexture(0)->readback(*device,0,0,{},bgra.data(),WOW*4,&error));
            pixels.presented.resize(WOW*WOH);
            for (unsigned i=0;i<bgra.size();++i)
                pixels.presented[i] = {bgra[i][2]/255.f,bgra[i][1]/255.f,bgra[i][0]/255.f,bgra[i][3]/255.f};
            // The production resample vertex preserves native rows and the
            // presentation fragment samples source at that UV. CAMetalLayer's
            // later Y flip is outside this offscreen fixture. Verify using the
            // actual asymmetric ledge/checker/clear, not a presumed ROI origin.
            const auto display_pixels=waterReadDisplay(*displayed->colorTexture(0),*device,error);
            for(unsigned y=0;y<WOH;++y)for(unsigned x=0;x<WOW;++x)for(unsigned c=0;c<3;++c)
            {
                const auto i=y*WOW+x;
                pixels.presented_same_row_max_error=std::max(pixels.presented_same_row_max_error,
                    double(std::abs(pixels.presented[i][c]-display_pixels[i][c])));
                pixels.presented_flipped_row_mae+=std::abs(pixels.presented[i][c]-display_pixels[(WOH-1-y)*WOW+x][c])/(WOW*WOH*3);
            }
            REQUIRE(pixels.presented_same_row_max_error<=1./255.+.000001);
            REQUIRE(pixels.presented_flipped_row_mae>.01);
            pixels.aurora_reset=snapshot.reset; pixels.aurora_frame_index=snapshot.frame_index;
            pixels.aurora_ticket=snapshot.ticket; pixels.aurora_write_parity=snapshot.write_parity;
            pixels.aurora_read_parity=snapshot.read_parity;
            REQUIRE(snapshot.jitter[0]==pixels.jitter[0]); REQUIRE(snapshot.jitter[1]==pixels.jitter[1]);
            std::copy_n(snapshot.previous_jitter,2,pixels.aurora_previous_jitter.begin());
        }
#else
        static_cast<void>(capture_diagnostics);
#endif
        if(advance_ssr_history)
        {
            // Observe the exact packet consumed by this completed world before
            // the normal publisher may overwrite its compatible history target.
            if(capture_diagnostics)
            {
                pixels.ssr_source_color_packed.resize(WW*WH);pixels.ssr_source_depth.resize(WW*WH);
                REQUIRE(ssr_history.target()->colorTexture(0)->readback(*device,0,0,{},pixels.ssr_source_color_packed.data(),WW*4,&error));
                REQUIRE(ssr_history.target()->colorTexture(1)->readback(*device,0,0,{},pixels.ssr_source_depth.data(),WW*4,&error));
                pixels.ssr_water_coverage_used=last_published_water_coverage;
            }
            if(!skip_ssr_publication)
            {
                ALMetalWorldScreenSpaceReflectionCoordinates c;
                auto raster=p;alMetalApplyTemporalJitter(raster.data(),pixels.jitter[0],pixels.jitter[1],WW,WH);
                const auto inverse_raster=waterMatrix(simd_inverse(waterSimd(raster.data())));
                std::copy(raster.begin(),raster.end(),c.projection);std::copy(inverse_raster.begin(),inverse_raster.end(),c.inverse_projection);
                std::copy(v.begin(),v.end(),c.modelview);c.reverse_z=true;c.source_world=c.camera_generation=world;
                REQUIRE(renderer->updateScreenSpaceReflectionHistory(*final,true,c,ssr_history,&error));
                REQUIRE(ssr_history.valid());REQUIRE(ssr_history.coordinates());
                REQUIRE(std::equal(raster.begin(),raster.end(),ssr_history.coordinates()->projection));
                last_published_water_coverage=pixels.water_coverage;pixels.ssr_history_published=true;
            }
            else pixels.ssr_history_skipped=true;
            pixels.ssr_generation_after=ssr_history.generation();pixels.ssr_source_world_after=ssr_history.coordinates()->source_world;
        }
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

// Authored grazing reflection case. Only its caller selects this scene;
// the original fixture geometry, projection, animation and materials stay intact.
template<unsigned IW, unsigned IH, unsigned OW, unsigned OH>
void configureGrazingWaterReflector(WaterFixture<IW, IH, OW, OH>& f)
{
    f.configuration = WaterFixture<IW, IH, OW, OH>::SceneConfiguration::grazing_reflector;
    f.environment.water_plane[3] = 20.f;
    f.authored_fresnel_offset = .5f;
    f.ssr_reflector_in_main = true;
    f.advance_ssr_history = true;
    f.publishWaterSsrSource();

    Fixture::Geometry wall{};
    const float positions[4][3]{
        {-52.f, 15.f, 4.f}, {63.f, 15.f, 4.f},
        {63.f, 15.f, 20.f}, {-52.f, 15.f, 20.f}
    };
    for (unsigned i = 0; i < 4; ++i)
    {
        std::copy_n(positions[i], 3, wall.positions[i]);
        wall.positions[i][3] = 1.f;
        wall.normals[i][1] = -1.f;
        wall.tangents[i][0] = wall.tangents[i][3] = 1.f;
        wall.uv[i][0] = i == 1 || i == 2;
        wall.uv[i][1] = i >= 2;
        std::fill_n(wall.colors[i], 4, 255);
    }
    const std::uint16_t indices[6]{0, 1, 2, 0, 2, 3};
    f.ssr_reflector.vertices = f.buffer(&wall, sizeof(wall));
    f.ssr_reflector.indices = f.buffer(indices, sizeof(indices));
    f.floor = f.card(-250.f, 250.f, -60.f, 350.f, -2.f, f.color, 9001);
    f.foreground = f.card(500.f, 501.f, 500.f, 501.f, .5f, f.color, 9002);

    // Real mip levels retain the production sampler's derivative contract.
    ALMetalTextureDescriptor descriptor;
    descriptor.width = descriptor.height = 32;
    descriptor.mipmap_level_count = 6;
    descriptor.format = ALMetalPixelFormat::rgba16_float;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    std::shared_ptr<ALMetalTexture> normals = ALMetalTexture::create(*f.device, descriptor, &f.error);
    REQUIRE(normals);
    auto upload = f.device->makeCommandBuffer("Authored water normal mip inputs", &f.error);
    REQUIRE(upload);
    std::vector<WHalf> pixels(32 * 32);
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 32; ++x)
            pixels[y * 32 + x] = {
                __fp16(.5f + .12f * std::sin(.51f*x + .17f*y)),
                __fp16(.5f + .11f * std::cos(.29f*x - .43f*y)),
                __fp16(1.f), __fp16(1.f)
            };
    for (unsigned level = 0, width = 32; level < 6; ++level, width = std::max(1u, width / 2))
    {
        REQUIRE(normals->upload(*upload, level, 0, {}, pixels.data(), width * 8, &f.error));
        if (width == 1) continue;
        const unsigned next = width / 2;
        std::vector<WHalf> mip(next * next);
        for (unsigned y = 0; y < next; ++y)
            for (unsigned x = 0; x < next; ++x)
                for (unsigned c = 0; c < 4; ++c)
                    mip[y * next + x][c] = __fp16((
                        float(pixels[(2*y)*width + 2*x][c]) +
                        float(pixels[(2*y)*width + 2*x + 1][c]) +
                        float(pixels[(2*y + 1)*width + 2*x][c]) +
                        float(pixels[(2*y + 1)*width + 2*x + 1][c])) * .25f);
        pixels = std::move(mip);
    }
    REQUIRE(upload->commitAndWait(&f.error));
    f.resources->textures.push_back(normals);
    f.normal = normals.get();
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
        WaterFixture<> actual, expected;
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

TEST_CASE("Temporal water inputs follow actual color coverage and foreground depth",
          "[almetal][temporal][water][water-inputs]")
{
    @autoreleasepool
    {
        WaterFixture<> visible, hole, absent;
        double maximum_water_depth_error=0,maximum_water_camera_motion_error=0;
        unsigned actual_water=0,uncovered_floor=0,protected_foreground=0;
        const auto projection=waterProjection();
        const auto inverse=waterSimd(waterMatrix(simd_inverse(waterSimd(projection.data()))).data());
        for(unsigned frame=0;frame<8;++frame)
        {
            const float camera=frame<2 ? 0.f : .02f;
            const float previous_camera=frame<3 ? 0.f : .02f;
            const float animation=frame<4 ? 0.f : .017f*(frame-3);
            const auto a=visible.render(false,camera,animation),b=hole.render(false,camera,animation,true,false),
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
        WaterFixture<> plane_policy,surface_policy;
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
            const auto a=plane_policy.render(false,0.f,0.f,true,true,false),b=surface_policy.render();
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
        WaterFixture<> baseline, zero, partial, full, behind, dry, behind_dry;
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
            const auto z = zero.render(false,0,0,true,true,true,&zero_pane);
            const auto p = partial.render(false,0,0,true,true,true,&partial_pane);
            const auto f = full.render(false,0,0,true,true,true,&full_pane);
            const auto b = behind.render(false,0,0,true,true,true,&behind_pane);
            const auto d = dry.render(false,0,0,false);
            const auto bd = behind_dry.render(false,0,0,false,true,true,&dry_pane);
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
        const auto invalid=full.render(false,0,0,true,true,true,&full_pane,true);
        REQUIRE(absent.phase==invalid.phase); REQUIRE_FALSE(invalid.pane_previous_valid);
        for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)
        {
            const auto i=y*WW+x;
            CHECK(invalid.unsafe[i]==255); // Full opacity does not grant history to an invalid previous pose.
            CHECK(std::abs(invalid.depth[i]-foreground_depth(invalid,x,y))<.000002);
        }
        const auto recovered=full.render(false,0,0,true,true,true,&full_pane);
        REQUIRE(recovered.pane_previous_valid);
        for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)CHECK(recovered.unsafe[y*WW+x]==0);
        CHECK(full.history.stats().resets==resets);

        // Every render above settles through its ordinary completion before
        // returning. Separate warm-up from steady fixed-descriptor worlds so
        // legitimate initial allocation is not confused with target churn.
        const auto before_warm=alMetalResourceAllocationStats().textures_created;
        for(unsigned i=0;i<8;++i)full.render(false,0,0,true,true,true,&full_pane);
        const auto warm_created=alMetalResourceAllocationStats().textures_created;
        const auto warm_pool=full.renderer->renderTargetPoolStats();
        for(unsigned i=0;i<8;++i)
        {
            const auto settled=full.render(false,0,0,true,true,true,&full_pane);
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

#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
TEST_CASE("Water detail traces deterministic jitter cycles through native reconstruction and accumulation",
          "[almetal][temporal][water][water-detail-cycles]")
{
    @autoreleasepool
    {
        WaterFixture<> f;
        constexpr unsigned warm=72, fixed_end=180, animated_end=288, moving_end=306, end=414;
        // Use the exact last-moving expression for the plateau. A literal .36f
        // differs from .02f*18 by one float ULP, creating a real first-stop motion
        // rather than the fully stationary phase reference required here.
        constexpr float stopped_camera=.02f*float(moving_end-animated_end);
        const float last_moving_camera=.02f*float((moving_end-1)-animated_end+1);
        REQUIRE(stopped_camera==last_moving_camera);
        std::filesystem::path directory;
        std::ofstream frame_csv,pixel_csv,cycle_csv;
        if(!f.directory.empty())
        {
            directory=f.directory/"detail-cycles-baseline";
            std::filesystem::create_directories(directory);
            frame_csv.open(directory/"frames.csv");pixel_csv.open(directory/"pixels.csv");cycle_csv.open(directory/"cycles.csv");
            frame_csv << "index,segment,cycle,phase,camera_x,animation,reset,frame_index,ticket,write_parity,read_parity,jitter_x,jitter_y,previous_jitter_x,previous_jitter_y,raw_contrast,raw_detail,native_contrast,native_detail,resolved_contrast,resolved_detail,presented_contrast,presented_detail,native_resolved_rgb_max,raw_same_phase_rgb_max,generated_nonzero,generated_classification_mismatch,generated_threshold_ambiguous,masks_reactive_mean,masks_disocclusion_mean,masks_shading_mean,masks_accumulation_mean,previous_accumulation_local_mean,next_accumulation_mean,instability_mean,lock_mean,presented_same_row_max_error,presented_flipped_row_mae\n";
            pixel_csv << "index,segment,cycle,phase,x,y,tone_delta,generated_r8,shading_half_nearest_r8,mask_reactive_r8,mask_disocclusion_r8,mask_shading_r8,mask_accumulation_r8,previous_accumulation_local_r8,next_accumulation_r8,dilated_motion_uv_x,dilated_motion_uv_y,luma_instability,internal_lock,unsafe_r8\n";
            cycle_csv << "segment,cycle,native_detail_min,native_detail_max,native_detail_min_phase,native_detail_max_phase,presented_detail_min,presented_detail_max\n";
        }
        struct Measures { double contrast=0,detail=0; };
        const auto measure=[](const std::vector<WColor>& pixels,unsigned width,bool input)
        {
            const unsigned x0=input?59:90,x1=input?66:99,y0=input?33:50,y1=input?38:56;
            double sum=0,squares=0,gradient=0;unsigned count=0;
            for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x)
            {
                const auto i=y*width+x;
                const double luma=.2126*pixels[i][0]+.7152*pixels[i][1]+.0722*pixels[i][2];
                sum+=luma;squares+=luma*luma;++count;
                for(unsigned c=0;c<3;++c)gradient+=std::abs(pixels[i][c]-pixels[i+1][c]);
            }
            return Measures{std::sqrt(std::max(0.,squares/count-(sum/count)*(sum/count))),gradient/(count*3)};
        };
        const auto rgb_max=[](const std::vector<WColor>& a,const std::vector<WColor>& b)
        {
            REQUIRE(a.size()==b.size());double maximum=0;
            for(unsigned i=0;i<a.size();++i)for(unsigned c=0;c<3;++c)maximum=std::max(maximum,double(std::abs(a[i][c]-b[i][c])));
            return maximum;
        };
        const auto rgb_mae=[](const std::vector<WColor>& a,const std::vector<WColor>& b)
        {
            REQUIRE(a.size()==b.size());double sum=0;
            for(unsigned i=0;i<a.size();++i)for(unsigned c=0;c<3;++c)sum+=std::abs(a[i][c]-b[i][c]);
            return sum/(a.size()*3);
        };
        const auto tone=[](const WColor& value)
        {
            const float scale=1.f+std::max(0.f,std::max(value[0],std::max(value[1],value[2])));
            return std::array<float,3>{std::max(0.f,value[0])/scale,std::max(0.f,value[1])/scale,std::max(0.f,value[2])/scale};
        };
        struct Reference
        {
            std::vector<WColor> raw,opaque;
            std::vector<float> depth;
            std::vector<std::array<__fp16,2>> motion;
            std::vector<std::uint8_t> unsafe;
        };
        std::array<Reference,WP> fixed_reference,stop_reference;
        std::array<double,WP> native_cycle{},presented_cycle{};
        double maximum_native_resolved=0,maximum_same_phase_input=0,maximum_animation_change=0;
        unsigned nonfinite=0,static_input_mismatches=0,reactive_transitions=0,threshold_mismatches=0;
        std::vector<std::uint8_t> previous_generated;
        unsigned previous_phase=WP-1;
        std::array<float,2> previous_jitter{};
        for(unsigned frame=0;frame<end;++frame)
        {
            const char* segment=frame<warm?"warm":frame<fixed_end?"fixed":frame<animated_end?"animated":frame<moving_end?"moving":"stopped";
            const unsigned start=frame<warm?0:frame<fixed_end?warm:frame<animated_end?fixed_end:frame<moving_end?animated_end:moving_end;
            const unsigned cycle=(frame-start)/WP;
            const float animation=frame<fixed_end?0.f:frame<animated_end?.017f*(frame-fixed_end+1):.017f*(animated_end-fixed_end);
            const float camera=frame<animated_end?0.f:frame<moving_end?.02f*(frame-animated_end+1):stopped_camera;
            const auto p=f.render(false,camera,animation,true,true,true,nullptr,false,true);
            CAPTURE(frame,segment,cycle,p.phase);
            REQUIRE(p.phase==(previous_phase+1)%WP);previous_phase=p.phase;
            REQUIRE(p.aurora_frame_index==float(frame));REQUIRE(p.aurora_reset==(frame==0));
            REQUIRE(p.aurora_read_parity!=p.aurora_write_parity);
            if(frame>0)CHECK(p.aurora_previous_jitter==previous_jitter);
            previous_jitter=p.jitter;
            for(const auto* image:{&p.raw,&p.opaque,&p.immediate,&p.output,&p.presented,&p.internal_history,&p.luma_history})
                for(const auto& pixel:*image)for(float v:pixel)nonfinite+=!std::isfinite(v);
            for(float v:p.luma_instability)nonfinite+=!std::isfinite(v);
            const auto raw=measure(p.raw,WW,true),native=measure(p.immediate,WOW,false),resolved=measure(p.output,WOW,false),final=measure(p.presented,WOW,false);
            const auto boundary_error=rgb_max(p.immediate,p.output);
            maximum_native_resolved=std::max(maximum_native_resolved,boundary_error);
            double same_phase=0;
            const bool fixed_segment=frame>=warm && frame<fixed_end,stopped_segment=frame>=moving_end;
            if(fixed_segment || stopped_segment)
            {
                auto& reference=(fixed_segment?fixed_reference:stop_reference)[p.phase];
                if(cycle==0)reference={p.raw,p.opaque,p.depth,p.motion,p.unsafe};
                else
                {
                    same_phase=rgb_max(p.raw,reference.raw);
                    maximum_same_phase_input=std::max(maximum_same_phase_input,same_phase);
                    static_input_mismatches+=p.raw!=reference.raw || p.opaque!=reference.opaque || p.depth!=reference.depth || p.motion!=reference.motion || p.unsafe!=reference.unsafe;
                }
            }
            if(frame>=fixed_end && frame<animated_end)
                maximum_animation_change=std::max(maximum_animation_change,rgb_mae(p.raw,fixed_reference[p.phase].raw));
            unsigned generated=0,classification_mismatch=0,ambiguous=0;
            double reactive=0,disocclusion=0,shading=0,accumulation=0,previous_accum=0,next_accum=0,instability=0,lock=0;
            for(unsigned y=33;y<38;++y)for(unsigned x=59;x<66;++x)
            {
                const auto i=y*WW+x;
                const auto pre=tone(p.opaque[i]),post=tone(p.raw[i]);
                const float delta=std::max(std::abs(pre[0]-post[0]),std::max(std::abs(pre[1]-post[1]),std::abs(pre[2]-post[2])));
                generated+=p.reactive[i]!=0;
                const bool near_threshold=std::abs(delta-.2f)<=.000002f;
                ambiguous+=near_threshold;
                if(!near_threshold)classification_mismatch+=bool(p.reactive[i])!=(delta>=.2f);
                if(fixed_segment && cycle>0 && !previous_generated.empty())reactive_transitions+=p.reactive[i]!=previous_generated[i];
                reactive+=p.masks[i][0]/255.;disocclusion+=p.masks[i][1]/255.;shading+=p.masks[i][2]/255.;accumulation+=p.masks[i][3]/255.;
                previous_accum+=p.previous_accumulation[i]/255.;next_accum+=p.accumulation[i]/255.;instability+=p.luma_instability[i];
                const unsigned hx=std::min(unsigned((x+.5f)*WOW/WW),WOW-1),hy=std::min(unsigned((y+.5f)*WOH/WH),WOH-1);
                const float current_lock=p.internal_history[hy*WOW+hx][3];lock+=current_lock;
                if(pixel_csv)pixel_csv<<std::setprecision(10)<<frame<<','<<segment<<','<<cycle<<','<<p.phase<<','<<x<<','<<y<<','<<delta<<','<<unsigned(p.reactive[i])<<','<<unsigned(p.shading_change[(y/2)*(WW/2)+x/2])<<','
                    <<unsigned(p.masks[i][0])<<','<<unsigned(p.masks[i][1])<<','<<unsigned(p.masks[i][2])<<','<<unsigned(p.masks[i][3])<<','<<unsigned(p.previous_accumulation[i])<<','<<unsigned(p.accumulation[i])<<','<<float(p.dilated_motion[i][0])<<','<<float(p.dilated_motion[i][1])<<','<<p.luma_instability[i]<<','<<current_lock<<','<<unsigned(p.unsafe[i])<<'\n';
            }
            previous_generated=p.reactive;threshold_mismatches+=classification_mismatch;
            if(frame_csv)frame_csv<<std::setprecision(10)<<frame<<','<<segment<<','<<cycle<<','<<p.phase<<','<<camera<<','<<animation<<','<<p.aurora_reset<<','<<p.aurora_frame_index<<','<<p.aurora_ticket<<','<<p.aurora_write_parity<<','<<p.aurora_read_parity<<','<<p.jitter[0]<<','<<p.jitter[1]<<','<<p.aurora_previous_jitter[0]<<','<<p.aurora_previous_jitter[1]<<','
                <<raw.contrast<<','<<raw.detail<<','<<native.contrast<<','<<native.detail<<','<<resolved.contrast<<','<<resolved.detail<<','<<final.contrast<<','<<final.detail<<','<<boundary_error<<','<<same_phase<<','<<generated<<','<<classification_mismatch<<','<<ambiguous<<','<<reactive/35<<','<<disocclusion/35<<','<<shading/35<<','<<accumulation/35<<','<<previous_accum/35<<','<<next_accum/35<<','<<instability/35<<','<<lock/35<<','<<p.presented_same_row_max_error<<','<<p.presented_flipped_row_mae<<'\n';
            native_cycle[p.phase]=native.detail;presented_cycle[p.phase]=final.detail;
            if((frame-start+1)%WP==0)
            {
                const auto lo=std::min_element(native_cycle.begin(),native_cycle.end()),hi=std::max_element(native_cycle.begin(),native_cycle.end());
                if(cycle_csv)cycle_csv<<std::setprecision(10)<<segment<<','<<cycle<<','<<*lo<<','<<*hi<<','<<std::distance(native_cycle.begin(),lo)<<','<<std::distance(native_cycle.begin(),hi)<<','
                    <<*std::min_element(presented_cycle.begin(),presented_cycle.end())<<','<<*std::max_element(presented_cycle.begin(),presented_cycle.end())<<'\n';
            }
            if(!directory.empty() && frame>=warm)
            {
                const auto prefix=directory/("world-"+std::to_string(frame));
                waterRaw(prefix.string()+"-hdr.rgba32f",p.raw);waterRaw(prefix.string()+"-opaque.rgba32f",p.opaque);
                waterRaw(prefix.string()+"-native.rgba32f",p.immediate);waterRaw(prefix.string()+"-resolved.rgba32f",p.output);waterRaw(prefix.string()+"-presented.rgba32f",p.presented);
                waterRaw(prefix.string()+"-depth.r32f",p.depth);waterRaw(prefix.string()+"-motion.rg16f",p.motion);waterRaw(prefix.string()+"-unsafe.r8",p.unsafe);
                waterRaw(prefix.string()+"-dilated-motion.rg16f",p.dilated_motion);
                waterRaw(prefix.string()+"-reactive.r8",p.reactive);waterRaw(prefix.string()+"-shading.r8",p.shading_change);waterRaw(prefix.string()+"-masks.rgba8",p.masks);
                waterRaw(prefix.string()+"-accumulation.r8",p.accumulation);waterRaw(prefix.string()+"-previous-accumulation.r8",p.previous_accumulation);
                waterRaw(prefix.string()+"-instability.r32f",p.luma_instability);waterRaw(prefix.string()+"-luma-history.rgba32f",p.luma_history);waterRaw(prefix.string()+"-internal-history.rgba32f",p.internal_history);
            }
        }
        CAPTURE(nonfinite,static_input_mismatches,threshold_mismatches,reactive_transitions,maximum_native_resolved,maximum_same_phase_input,maximum_animation_change);
        CHECK(nonfinite==0);CHECK(static_input_mismatches==0);CHECK(threshold_mismatches==0);
        CHECK(maximum_animation_change>.00002); // Retain the existing genuine normal-animation signal bound.
        CHECK(f.history.stats().resets==1);CHECK(f.aurora->stats().reset_calls==1);
        std::cout<<"Water detail414worlds: static input mismatches="<<static_input_mismatches
            <<", generated threshold mismatches="<<threshold_mismatches<<", generated ROI transitions="<<reactive_transitions
            <<", native/resolved RGB max="<<maximum_native_resolved<<", same-phase HDR max="<<maximum_same_phase_input
            <<", genuine normal-animation delta="<<maximum_animation_change<<'\n';
        // Repeated phases retain source/history stability observations without
        // inventing an owner-water quality bound. The separate SSR regression
        // below guards the demonstrated source-sampling noise improvement.
    }
}
#endif

#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
TEST_CASE("Actual water rough SSR is compared with a fixed high-budget reference before and after reconstruction",
          "[almetal][temporal][water][water-ssr-detail]")
{
    @autoreleasepool
    {
        WaterFixture<> low,high,off;
        // The old .1 offset attenuates reflected radiance twice and makes
        // hits invisible at RGBA16F precision in this downward-facing view.
        // Author reflective water explicitly (the owner scene uses .5), held
        // identical across all arms; old non-SSR fixture material is unchanged.
        low.authored_fresnel_offset=high.authored_fresnel_offset=off.authored_fresnel_offset=.5f;
        low.publishWaterSsrSource();high.publishWaterSsrSource();off.publishWaterSsrSource();
        constexpr unsigned warm=72,fixed_end=126,animated_end=180,moving_end=198,end=252;
        constexpr float stopped_camera=.02f*float(moving_end-animated_end);
        REQUIRE(stopped_camera==.02f*float((moving_end-1)-animated_end+1));
        std::filesystem::path directory;
        std::ofstream csv;
        if(!low.directory.empty())
        {
            directory=low.directory/"water-ssr-detail";std::filesystem::create_directories(directory);
            csv.open(directory/"frames.csv");REQUIRE(csv.good());csv<<std::setprecision(12);
            csv<<"world,segment,cycle,phase,camera_x,animation,raw_low_mean,raw_reference_mean,raw_low_mae,raw_centered_rms,raw_residual_gradient_rms,raw_low_ssr_contribution,raw_reference_ssr_contribution,native_low_mean,native_reference_mean,native_mae,native_centered_rms,native_residual_gradient_rms,presented_low_mean,presented_reference_mean,presented_mae,presented_centered_rms,presented_residual_gradient_rms,raw_low_phase_mae,raw_reference_phase_mae\n";
        }
        struct Measures{double a=0,b=0,mae=0,rms=0,gradient=0;};
        const auto measure=[](const std::vector<WColor>& a,const std::vector<WColor>& b,unsigned width,bool input)
        {
            REQUIRE(a.size()==b.size());const unsigned x0=input?59:90,x1=input?66:99,y0=input?33:50,y1=input?38:56;
            Measures m;double sum=0,square=0;unsigned count=0;
            for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x)for(unsigned c=0;c<3;++c)
            {
                const auto i=y*width+x;const double residual=a[i][c]-b[i][c];
                m.a+=a[i][c];m.b+=b[i][c];m.mae+=std::abs(residual);sum+=residual;square+=residual*residual;
                m.gradient+=std::pow(residual-double(a[i+1][c]-b[i+1][c]),2);++count;
            }
            m.a/=count;m.b/=count;m.mae/=count;m.rms=std::sqrt(std::max(0.,square/count-std::pow(sum/count,2)));m.gradient=std::sqrt(m.gradient/count);
            return m;
        };
        std::array<std::vector<WColor>,WP> low_fixed,high_fixed,low_stopped,high_stopped;
        double maximum_low_contribution=0,maximum_reference_contribution=0,minimum_reference_contribution=1.e30;
        double low_error_sum=0,raw_residual_sum=0;unsigned measured=0;
        for(unsigned frame=0;frame<end;++frame)
        {
            const char* segment=frame<warm?"warm":frame<fixed_end?"fixed":frame<animated_end?"animated":frame<moving_end?"moving":"stopped";
            const unsigned begin=frame<warm?0:frame<fixed_end?warm:frame<animated_end?fixed_end:frame<moving_end?animated_end:moving_end;
            const unsigned cycle=(frame-begin)/WP;
            const float animation=frame<fixed_end?0.f:frame<animated_end?.017f*(frame-fixed_end+1):.017f*(animated_end-fixed_end);
            const float camera=frame<animated_end?0.f:frame<moving_end?.02f*(frame-animated_end+1):stopped_camera;
            auto lp=low.waterSsr(camera,4),hp=high.waterSsr(camera,128);
            const auto render=[&](WaterFixture<>& f,const ALMetalWorldScreenSpaceReflections* ssr)
            {return f.render(false,camera,animation,true,true,true,nullptr,false,true,ssr);};
            const auto l=render(low,&lp),h=render(high,&hp),o=render(off,nullptr);
            CAPTURE(frame,segment,cycle,l.phase);
            REQUIRE(l.phase==h.phase);REQUIRE(l.phase==o.phase);REQUIRE(l.jitter==h.jitter);REQUIRE(l.jitter==o.jitter);
            REQUIRE(l.raw.size()==WW*WH);REQUIRE(h.immediate.size()==WOW*WOH);REQUIRE(h.presented.size()==WOW*WOH);
            if(frame<warm)continue;
            const auto raw=measure(l.raw,h.raw,WW,true),native=measure(l.immediate,h.immediate,WOW,false),presented=measure(l.presented,h.presented,WOW,false);
            const auto contribution=measure(l.raw,o.raw,WW,true).mae,reference_contribution=measure(h.raw,o.raw,WW,true).mae;
            maximum_low_contribution=std::max(maximum_low_contribution,contribution);
            maximum_reference_contribution=std::max(maximum_reference_contribution,reference_contribution);
            minimum_reference_contribution=std::min(minimum_reference_contribution,reference_contribution);
            // A no-hit or legacy-environment path cannot qualify this source.
            REQUIRE(reference_contribution>1.e-6);
            double low_phase=0,high_phase=0;
            if(frame<fixed_end || frame>=moving_end)
            {
                auto& lr=(frame<fixed_end?low_fixed:low_stopped)[l.phase];auto& hr=(frame<fixed_end?high_fixed:high_stopped)[h.phase];
                if(cycle==0){lr=l.raw;hr=h.raw;}else{low_phase=measure(l.raw,lr,WW,true).mae;high_phase=measure(h.raw,hr,WW,true).mae;}
            }
            if(csv)csv<<frame<<','<<segment<<','<<cycle<<','<<l.phase<<','<<camera<<','<<animation<<','
                <<raw.a<<','<<raw.b<<','<<raw.mae<<','<<raw.rms<<','<<raw.gradient<<','<<contribution<<','<<reference_contribution<<','
                <<native.a<<','<<native.b<<','<<native.mae<<','<<native.rms<<','<<native.gradient<<','
                <<presented.a<<','<<presented.b<<','<<presented.mae<<','<<presented.rms<<','<<presented.gradient<<','<<low_phase<<','<<high_phase<<'\n';
            if(!directory.empty())
            {
                for(const auto& arm:std::array<std::pair<const char*,const WaterPixels*>,3>{{{"budget4",&l},{"reference128",&h},{"ssr-off",&o}}})
                {
                    const auto prefix=directory/(std::string(arm.first)+"-world-"+std::to_string(frame));
                    waterRaw(prefix.string()+"-hdr.rgba32f",arm.second->raw);
                    waterRaw(prefix.string()+"-native.rgba32f",arm.second->immediate);
                    waterRaw(prefix.string()+"-presented.rgba32f",arm.second->presented);
                }
            }
            ++measured;low_error_sum+=raw.mae;raw_residual_sum+=raw.rms;
        }
        CAPTURE(maximum_low_contribution,maximum_reference_contribution,minimum_reference_contribution);
        CHECK(maximum_low_contribution>1.e-6);CHECK(maximum_reference_contribution>1.e-6);
        REQUIRE(measured==180);
        // Frozen 32-ray reference: original budget4/one-ray baseline measured
        // MAE .0117946 and centered RMS .020513; minimum-four measured
        // .00561454/.0100094. These guard source SSR noise, not owner-water
        // reconstruction cycling or a physically exact reflection solution.
        CHECK(low_error_sum/measured<.008);
        CHECK(raw_residual_sum/measured<.014);
        std::cout<<"Water SSR budget4/reference128: "<<measured<<" measured worlds after72warm, mean raw MAE="<<low_error_sum/measured
            <<", centered residual RMS="<<raw_residual_sum/measured<<", SSR contribution range="<<minimum_reference_contribution<<".."<<maximum_reference_contribution<<'\n';
        // Characterization only: 128 is an unchanged high-budget instance of
        // this shader, not an independent physically exact reflection oracle.
        // No new reconstruction-quality bound is invented by this fixture.
    }
}
#endif

#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
TEST_CASE("Water-inclusive SSR history advances with the exact submitted jittered world",
          "[almetal][temporal][water][water-ssr-history]")
{
    @autoreleasepool
    {
        WaterFixture<> f,off;
        for(auto* fixture:{&f,&off})
        {
            fixture->authored_fresnel_offset=.5f;fixture->ssr_reflector_in_main=true;
            fixture->advance_ssr_history=true;fixture->publishWaterSsrSource();
        }
        constexpr unsigned skipped_publication_frame=261;
        constexpr unsigned warm=72,fixed_end=180,animated_end=252,moving_end=270,end=342;
        constexpr float stopped_camera=.02f*float(moving_end-animated_end);
        REQUIRE(stopped_camera==.02f*float((moving_end-1)-animated_end+1));
        std::filesystem::path directory;std::ofstream csv,coordinates_csv;
        if(!f.directory.empty())
        {
            directory=f.directory/"water-ssr-history-advancing";
            std::filesystem::create_directories(directory);csv.open(directory/"frames.csv");REQUIRE(csv.good());
            coordinates_csv.open(directory/"used-coordinates.csv");REQUIRE(coordinates_csv.good());
            coordinates_csv<<"index,generation,source_world,camera_generation,reverse_z,origin_x,origin_y,origin_z";
            for(const char* matrix:{"projection","inverse_projection","modelview"})for(unsigned i=0;i<16;++i)coordinates_csv<<','<<matrix<<i;
            coordinates_csv<<'\n';
            csv<<"index,world,segment,cycle,phase,jitter_x,jitter_y,camera_x,animation,reset,skip_publication,source_generation_used,source_world_used,source_generation_after,source_world_after,published,skipped,raw_detail,native_detail,presented_detail,raw_same_phase_mae,native_same_phase_mae,native_consecutive_mae,generated_reactive_mean,mask_reactive_mean,mask_shading_mean,mask_accumulation_mean,history_water_fraction\n";
        }
        const auto detail=[](const std::vector<WColor>& a,unsigned width,bool input)
        {
            double sum=0;unsigned count=0;const unsigned x0=input?59:90,x1=input?66:99,y0=input?33:50,y1=input?36:54;
            for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x)for(unsigned c=0;c<3;++c)
            {sum+=std::abs(a[y*width+x][c]-a[y*width+x+1][c]);++count;}
            return sum/count;
        };
        const auto mae=[](const std::vector<WColor>& a,const std::vector<WColor>& b,unsigned width,bool input)
        {
            REQUIRE(a.size()==b.size());double sum=0;unsigned count=0;
            const unsigned x0=input?59:90,x1=input?66:99,y0=input?33:50,y1=input?36:54;
            for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x)for(unsigned c=0;c<3;++c)
            {sum+=std::abs(a[y*width+x][c]-b[y*width+x][c]);++count;}
            return sum/count;
        };
        std::array<std::vector<WColor>,WP> previous_phase_raw,previous_phase_native;
        std::vector<WColor> previous_native;unsigned published=0,skipped=0;double ssr_contribution=0;
        const auto initial_generation=f.ssr_history.generation();std::uint64_t last_generation=initial_generation,last_source=f.ssr_history.coordinates()->source_world;
        std::vector<std::uint32_t> skipped_color;std::vector<float> skipped_depth;
        std::vector<std::uint8_t> skipped_coverage;ALMetalWorldScreenSpaceReflectionCoordinates skipped_coordinates;
        // Frozen before the receiver-seed qualification run: the old source
        // gives temporal luma RMS .007548; target <.003. A fixed global lobe
        // was rejected for brightness bias. The independent retained32-ray
        // comparison has mean .269994; preserve that appearance within3%.
        std::array<double,36> stationary_luma_sum{},stationary_luma_square{};
        double stationary_spatial_contrast=0;unsigned stationary_worlds=0;
        for(unsigned frame=0;frame<end;++frame)
        {
            const char* segment=frame<warm?"warm":frame<fixed_end?"fixed":frame<animated_end?"animated":frame<moving_end?"moving":"stopped";
            const unsigned begin=frame<warm?0:frame<fixed_end?warm:frame<animated_end?fixed_end:frame<moving_end?animated_end:moving_end;
            const float animation=frame<fixed_end?0.f:.017f*(frame-fixed_end+1);
            const float camera=frame<animated_end?0.f:frame<moving_end?.02f*(frame-animated_end+1):stopped_camera;
            f.skip_ssr_publication=frame==skipped_publication_frame;
            auto ssr=f.waterSsr(camera,4);
            const auto p=f.render(false,camera,animation,true,true,true,nullptr,false,true,&ssr);
            CAPTURE(frame,segment,p.phase,f.skip_ssr_publication,p.ssr_source_world_used,p.ssr_source_world_after);
            REQUIRE(p.phase==frame%WP);REQUIRE(p.aurora_reset==(frame==0));
            REQUIRE(p.ssr_generation_used==last_generation);REQUIRE(p.ssr_source_world_used==last_source);
            if(f.skip_ssr_publication)
            {REQUIRE(p.ssr_history_skipped);REQUIRE_FALSE(p.ssr_history_published);REQUIRE(p.ssr_generation_after==last_generation);REQUIRE(p.ssr_source_world_after==last_source);++skipped;}
            else
            {REQUIRE(p.ssr_history_published);REQUIRE_FALSE(p.ssr_history_skipped);REQUIRE(p.ssr_generation_after==last_generation+1);REQUIRE(p.ssr_source_world_after==f.world);++published;}
            last_generation=p.ssr_generation_after;last_source=p.ssr_source_world_after;
            REQUIRE(p.ssr_source_color_packed.size()==WW*WH);REQUIRE(p.ssr_source_depth.size()==WW*WH);REQUIRE(p.ssr_water_coverage_used.size()==WW*WH);
            if(frame==skipped_publication_frame)
            {
                skipped_color=p.ssr_source_color_packed;skipped_depth=p.ssr_source_depth;
                skipped_coverage=p.ssr_water_coverage_used;skipped_coordinates=p.ssr_used_coordinates;
            }
            if(frame==skipped_publication_frame+1)
            {
                // Skipping publication holds the complete paired packet, not
                // only its serial; the next world consumes those exact texels
                // and the original jitter/camera coordinates without a reset.
                REQUIRE(p.ssr_source_color_packed==skipped_color);REQUIRE(p.ssr_source_depth==skipped_depth);
                REQUIRE(p.ssr_water_coverage_used==skipped_coverage);
                const auto& c=p.ssr_used_coordinates;
                REQUIRE(c.source_world==skipped_coordinates.source_world);REQUIRE(c.camera_generation==skipped_coordinates.camera_generation);
                REQUIRE(c.reverse_z==skipped_coordinates.reverse_z);
                REQUIRE(std::equal(c.world_origin,c.world_origin+3,skipped_coordinates.world_origin));
                REQUIRE(std::equal(c.projection,c.projection+16,skipped_coordinates.projection));
                REQUIRE(std::equal(c.inverse_projection,c.inverse_projection+16,skipped_coordinates.inverse_projection));
                REQUIRE(std::equal(c.modelview,c.modelview+16,skipped_coordinates.modelview));
            }
            for(unsigned y=33;y<36;++y)for(unsigned x=59;x<66;++x)REQUIRE(p.water_coverage[y*WW+x]==255);
            if(frame<=warm)
            {
                const auto no_ssr=off.render(false,camera,animation,true,true,true,nullptr,false,false,nullptr);
                REQUIRE(p.phase==no_ssr.phase);REQUIRE(p.jitter==no_ssr.jitter);
                if(frame==warm){ssr_contribution=mae(p.raw,no_ssr.raw,WW,true);REQUIRE(ssr_contribution>1.e-6);}
            }
            if(frame>=warm && frame<fixed_end)
            {
                unsigned n=0;double mean=0,square=0;
                for(unsigned y=50;y<54;++y)for(unsigned x=90;x<99;++x)
                {
                    const auto& c=p.presented[y*WOW+x];const double luma=.2126*c[0]+.7152*c[1]+.0722*c[2];
                    stationary_luma_sum[n]+=luma;stationary_luma_square[n]+=luma*luma;++n;
                    mean+=luma;square+=luma*luma;
                }
                REQUIRE(n==36);mean/=n;stationary_spatial_contrast+=std::sqrt(std::max(0.,square/n-mean*mean));++stationary_worlds;
            }
            const auto raw_detail=detail(p.raw,WW,true),native_detail=detail(p.immediate,WOW,false),presented_detail=detail(p.presented,WOW,false);
            REQUIRE(std::isfinite(raw_detail));REQUIRE(std::isfinite(native_detail));REQUIRE(std::isfinite(presented_detail));
            double raw_same=0,native_same=0,consecutive=0;
            if(!previous_phase_raw[p.phase].empty())
            {raw_same=mae(p.raw,previous_phase_raw[p.phase],WW,true);native_same=mae(p.immediate,previous_phase_native[p.phase],WOW,false);}
            if(!previous_native.empty())consecutive=mae(p.immediate,previous_native,WOW,false);
            previous_phase_raw[p.phase]=p.raw;previous_phase_native[p.phase]=p.immediate;previous_native=p.immediate;
            double generated=0,reactive=0,shading=0,accumulation=0;
            for(unsigned y=33;y<36;++y)for(unsigned x=59;x<66;++x)
            {const auto i=y*WW+x;generated+=p.reactive[i]/255.;reactive+=p.masks[i][0]/255.;shading+=p.masks[i][2]/255.;accumulation+=p.masks[i][3]/255.;}
            const double history_water=double(std::count_if(p.ssr_water_coverage_used.begin(),p.ssr_water_coverage_used.end(),[](auto x){return x!=0;}))/(WW*WH);
            if(csv)csv<<std::setprecision(12)<<frame<<','<<f.world<<','<<segment<<','<<(frame-begin)/WP<<','<<p.phase<<','<<p.jitter[0]<<','<<p.jitter[1]<<','<<camera<<','<<animation<<','<<p.aurora_reset<<','<<f.skip_ssr_publication<<','
                <<p.ssr_generation_used<<','<<p.ssr_source_world_used<<','<<p.ssr_generation_after<<','<<p.ssr_source_world_after<<','<<p.ssr_history_published<<','<<p.ssr_history_skipped<<','
                <<raw_detail<<','<<native_detail<<','<<presented_detail<<','<<raw_same<<','<<native_same<<','<<consecutive<<','<<generated/21<<','<<reactive/21<<','<<shading/21<<','<<accumulation/21<<','<<history_water<<'\n';
            if(coordinates_csv)
            {
                const auto& c=p.ssr_used_coordinates;
                coordinates_csv<<std::setprecision(12)<<frame<<','<<p.ssr_generation_used<<','<<c.source_world<<','<<c.camera_generation<<','<<c.reverse_z;
                for(double origin:c.world_origin)coordinates_csv<<','<<origin;
                for(const float* matrix:{c.projection,c.inverse_projection,c.modelview})for(unsigned i=0;i<16;++i)coordinates_csv<<','<<matrix[i];
                coordinates_csv<<'\n';
            }
            if(!directory.empty() && frame>=warm)
            {
                const auto prefix=directory/("world-"+std::to_string(frame));
                waterRaw(prefix.string()+"-hdr.rgba32f",p.raw);waterRaw(prefix.string()+"-opaque.rgba32f",p.opaque);
                waterRaw(prefix.string()+"-native.rgba32f",p.immediate);waterRaw(prefix.string()+"-resolved.rgba32f",p.output);waterRaw(prefix.string()+"-presented.rgba32f",p.presented);
                waterRaw(prefix.string()+"-water-coverage.r8",p.water_coverage);waterRaw(prefix.string()+"-water-depth.r32f",p.captured_water_depth);
                waterRaw(prefix.string()+"-depth.r32f",p.depth);waterRaw(prefix.string()+"-motion.rg16f",p.motion);waterRaw(prefix.string()+"-unsafe.r8",p.unsafe);
                waterRaw(prefix.string()+"-reactive.r8",p.reactive);waterRaw(prefix.string()+"-masks.rgba8",p.masks);waterRaw(prefix.string()+"-accumulation.r8",p.accumulation);
                if(frame<fixed_end)
                {
                    waterRaw(prefix.string()+"-ssr-used-color.rg11b10f",p.ssr_source_color_packed);waterRaw(prefix.string()+"-ssr-used-depth.r32f",p.ssr_source_depth);
                    waterRaw(prefix.string()+"-ssr-used-water-coverage.r8",p.ssr_water_coverage_used);
                }
            }
        }
        CHECK(published==end-1);CHECK(skipped==1);
        CHECK(f.history.stats().resets==1);CHECK(f.aurora->stats().reset_calls==1);
        REQUIRE(stationary_worlds==108);double stationary_variance=0,stationary_mean=0;
        for(unsigned i=0;i<36;++i)
        {
            const double mean=stationary_luma_sum[i]/stationary_worlds;stationary_mean+=mean;
            stationary_variance+=std::max(0.,stationary_luma_square[i]/stationary_worlds-mean*mean);
        }
        const double temporal_rms=std::sqrt(stationary_variance/36);stationary_mean/=36;
        stationary_spatial_contrast/=stationary_worlds;
        CAPTURE(temporal_rms,stationary_mean,stationary_spatial_contrast,ssr_contribution);
        CHECK(temporal_rms<.003);
        CHECK(std::abs(stationary_mean/.269994-1.)<.03);
        CHECK(stationary_spatial_contrast>.0042); // Reflected detail must not become a flat patch.
        CHECK(ssr_contribution>.012); // Omitting/fading SSR cannot satisfy the stability bound.
        std::cout<<"Stationary water temporal RMS="<<temporal_rms<<", mean="<<stationary_mean
            <<", spatial contrast="<<stationary_spatial_contrast<<'\n';
        std::cout<<"Water-inclusive SSR history342worlds: skipped-publication-index="<<skipped_publication_frame<<", publications="<<published<<", skipped="<<skipped<<", nonemptySSR MAE="<<ssr_contribution<<'\n';
        // This detects the source SSR phase cycle; owner-scene acceptance and
        // physically exact/high-budget reflection fidelity remain separate.
    }
}
#endif

#if ALCHEMY_TEMPORAL_WATER_JITTER_REPAIR
TEST_CASE("Grazing water SSR retains above-plane reflection across jitter phases",
          "[almetal][temporal][water][water-ssr-self-depth]")
{
    @autoreleasepool
    {
        constexpr unsigned width = 384, height = 256;
        constexpr unsigned output_width = 576, output_height = 384;
        WaterFixture<width, height, output_width, output_height> f;
        REQUIRE(f.projection() == waterProjection(width, height));
        REQUIRE(f.view(0.f) == waterView(0.f));
        configureGrazingWaterReflector(f);
        constexpr unsigned warm = 72, stationary_end = 144, moving_end = 162, end = 198;
        const float stopped_camera = .02f * float(moving_end - stationary_end);
        std::filesystem::path directory;
        std::ofstream csv;
        if (!f.directory.empty())
        {
            directory = f.directory / "authored-water-self-depth";
            std::filesystem::create_directories(directory);
            csv.open(directory / "frames.csv");
            REQUIRE(csv.good());
            csv << "index,world,phase,segment,source_world,history_generation,history_water_fraction,"
                   "raw_mean,raw_contrast,raw_detail,native_mean,native_detail,presented_mean,presented_detail,reset\n";
        }
        const auto metrics = [](const std::vector<WColor>& image, unsigned width, bool input)
        {
            const unsigned x0 = input ? 232 : 348, x1 = input ? 276 : 416;
            const unsigned y0 = input ? 124 : 184, y1 = input ? 164 : 248;
            std::array<double, 3> result{};
            double square = 0;
            unsigned count = 0, pairs = 0;
            for (unsigned y = y0; y < y1; ++y)
                for (unsigned x = x0; x < x1; ++x)
                {
                    const auto& pixel = image[y * width + x];
                    const double luma = .2126*pixel[0] + .7152*pixel[1] + .0722*pixel[2];
                    result[0] += luma;
                    square += luma*luma;
                    ++count;
                    for (unsigned c = 0; c < 3; ++c)
                    {
                        if (x + 1 < x1)
                        {
                            result[2] += std::abs(pixel[c] - image[y * width + x + 1][c]);
                            ++pairs;
                        }
                        if (y + 1 < y1)
                        {
                            result[2] += std::abs(pixel[c] - image[(y + 1) * width + x][c]);
                            ++pairs;
                        }
                    }
                }
            result[0] /= count;
            result[1] = std::sqrt(std::max(0., square/count - result[0]*result[0]));
            result[2] /= pairs;
            return result;
        };
        std::array<double, WP> phase_sum{};
        std::array<unsigned, WP> phase_count{};
        double detail_sum = 0, mean_sum = 0;
        unsigned stationary_worlds = 0, publications = 0;
        for (unsigned frame = 0; frame < end; ++frame)
        {
            const float camera = frame < stationary_end ? 0.f
                : frame < moving_end ? .02f * float(frame - stationary_end + 1) : stopped_camera;
            const float seconds = float(frame) / 30.f;
            auto reflections = f.waterSsr(camera, 4);
            const auto before = f.ssr_history.generation();
            const auto pixels = f.render(false, camera, seconds, true, true, true,
                nullptr, false, true, &reflections);
            REQUIRE(pixels.ssr_history_published);
            REQUIRE(pixels.ssr_generation_after == before + 1);
            REQUIRE(pixels.ssr_source_world_after == f.world);
            ++publications;
            if (frame > 0) REQUIRE_FALSE(pixels.aurora_reset);

            // Independently recover the authored flat water from its actual
            // raster depth. This verifies the source geometry, not a fake depth map.
            auto raster = f.projection();
            alMetalApplyTemporalJitter(raster.data(), pixels.jitter[0], pixels.jitter[1], width, height);
            const auto inverse_raster = simd_inverse(waterSimd(raster.data()));
            const auto inverse_view = simd_inverse(waterSimd(f.view(camera).data()));
            unsigned visible = 0;
            for (unsigned y = 124; y < 164; ++y)
                for (unsigned x = 232; x < 276; ++x)
                {
                    const auto index = y * width + x;
                    REQUIRE(pixels.water_coverage[index] == 255);
                    ++visible;
                    auto eye = simd_mul(inverse_raster, simd_make_float4(
                        2.f*(x + .5f)/width - 1.f, 2.f*(y + .5f)/height - 1.f,
                        pixels.captured_water_depth[index], 1.f));
                    eye /= eye.w;
                    const auto world = simd_mul(inverse_view, eye);
                    CHECK(std::abs(world.z / world.w) < .001f);
                }
            REQUIRE(visible == 1760);

            const auto raw = metrics(pixels.raw, width, true);
            const auto native = metrics(pixels.immediate, output_width, false);
            const auto presented = metrics(pixels.presented, output_width, false);
            for (const auto& values : {raw, native, presented})
                for (double value : values) REQUIRE(std::isfinite(value));
            const char* segment = frame < warm ? "warm" : frame < stationary_end ? "stationary"
                : frame < moving_end ? "moving" : "stopped";
            if (frame >= warm && frame < stationary_end)
            {
                phase_sum[pixels.phase] += raw[0];
                ++phase_count[pixels.phase];
                detail_sum += raw[2];
                mean_sum += raw[0];
                ++stationary_worlds;
            }
            if (csv)
                csv << std::setprecision(12) << frame << ',' << f.world << ',' << pixels.phase << ','
                    << segment << ',' << pixels.ssr_source_world_used << ',' << pixels.ssr_generation_used << ','
                    << double(std::count_if(pixels.ssr_water_coverage_used.begin(),
                           pixels.ssr_water_coverage_used.end(), [](auto value) { return value != 0; })) / (width*height)
                    << ',' << raw[0] << ',' << raw[1] << ',' << raw[2] << ',' << native[0] << ',' << native[2]
                    << ',' << presented[0] << ',' << presented[2] << ',' << pixels.aurora_reset << '\n';
            const bool selected_phase = pixels.phase == 7 || pixels.phase == 8 ||
                pixels.phase == 9 || pixels.phase == 17 || pixels.phase == 0;
            const bool save = (frame >= stationary_end - WP && frame < stationary_end && selected_phase) ||
                frame == 144 || frame == 152 || frame == 161 || frame == 162 ||
                frame == 179 || frame == 180 || frame == 197;
            if (save && !directory.empty())
            {
                const auto name = directory / ("frame-" + std::to_string(frame));
                waterRaw(name.string() + "-hdr.rgba32f", pixels.raw);
                waterRaw(name.string() + "-native.rgba32f", pixels.immediate);
                waterRaw(name.string() + "-presented.rgba32f", pixels.presented);
                waterRaw(name.string() + "-water-depth.r32f", pixels.captured_water_depth);
                waterRaw(name.string() + "-water-coverage.r8", pixels.water_coverage);
                waterRaw(name.string() + "-history-depth.r32f", pixels.ssr_source_depth);
                waterRaw(name.string() + "-history-water.r8", pixels.ssr_water_coverage_used);
            }
        }
        std::array<double, WP> phase_mean{};
        for (unsigned phase = 0; phase < WP; ++phase)
        {
            REQUIRE(phase_count[phase] == 4);
            phase_mean[phase] = phase_sum[phase] / phase_count[phase];
        }
        const auto [low, high] = std::minmax_element(phase_mean.begin(), phase_mean.end());
        const double range = *high - *low;
        const double jump_8_9 = std::abs(phase_mean[9] - phase_mean[8]);
        const double jump_17_0 = std::abs(phase_mean[0] - phase_mean[17]);
        const double detail = detail_sum / stationary_worlds;
        const double mean = mean_sum / stationary_worlds;
        CAPTURE(range, jump_8_9, jump_17_0, detail, mean);
        // Frozen before the candidate run in the dense visible-reflector case:
        // baseline range .0102627503255, jumps .0081304516605/.004024902264,
        // detail .00593085859117 and mean .143027696052. Require a material
        // pulse reduction while retaining at least 80% of detail and brightness.
        // The earlier 96x64 lookalike is retained as failed evidence; its coarse
        // gate and offscreen reflector did not reproduce this complete mechanism.
        CHECK(range < .005);
        CHECK(jump_8_9 < .004);
        CHECK(jump_17_0 < .004);
        CHECK(detail >= .004744686872937667);
        CHECK(mean >= .11442215684160001);
        CHECK(mean <= .17163323526239999);
        std::cout << "Grazing water SSR: publications=" << publications << ", range=" << range
            << ", jump8to9=" << jump_8_9 << ", jump17to0=" << jump_17_0
            << ", detail=" << detail << ", mean=" << mean << '\n';
        // This rendered counterexample does not replace owner-scene acceptance.
    }
}
#endif
