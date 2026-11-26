/** Production SSR publication and sampling must share the exact raster coordinates.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetaltemporalworldhistory.h"
#include <simd/simd.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>

// The preserved baseline fixture explicitly builds with0 against its retained
// libraries. The corrected test enables the typed publisher once available.
#ifndef ALCHEMY_SSR_COORDINATE_HISTORY
#define ALCHEMY_SSR_COORDINATE_HISTORY 1
#endif

namespace
{
constexpr unsigned SW = 96, SH = 64;
using SColor = std::array<float, 4>;
using SHalf = std::array<__fp16, 4>;
std::array<float, 16> ssrInverse(const std::array<float, 16>& value)
{
    simd_float4x4 matrix;
    std::memcpy(&matrix, value.data(), sizeof(matrix));
    const auto result = simd_inverse(matrix);
    std::array<float, 16> out;
    std::memcpy(out.data(), &result, sizeof(result));
    return out;
}
struct SsrRasterFixture : Fixture
{
    std::array<float, 16> projection{}, inverse{};
    ALMetalWorldReflectionProbes probes;
    const ALMetalTexture* light_map = nullptr;
    ALMetalWorldSimpleBatch landmark;
    std::unique_ptr<ALMetalRenderTarget> receiver;
    std::unique_ptr<ALMetalRenderTarget> raw_history;
#if ALCHEMY_SSR_COORDINATE_HISTORY
    ALMetalWorldScreenSpaceReflectionHistory typed_history;
#endif
    ALMetalTemporalWorldHistory phase_history;
    unsigned world = 0;

    SsrRasterFixture()
    {
        const float near = .1f, far = 10.f, cot = 1.f / std::tan(.5f);
        projection[0] = projection[5] = cot;
        projection[10] = near / (far - near);
        projection[11] = -1.f;
        projection[14] = far * near / (far - near);
        inverse = ssrInverse(projection);
        const auto make_texture = [&](ALMetalTextureDescriptor d, const void* pixels, unsigned row)
        {
            d.usage = ALMetalTextureUsage::shader_read;
            std::shared_ptr<ALMetalTexture> result = ALMetalTexture::create(*device, d, &error);
            REQUIRE(result);
            auto command = device->makeCommandBuffer("SSR raster authored inputs", &error);
            REQUIRE(command);
            const unsigned faces = d.type == ALMetalTextureType::texture_cube_array ? 6 : 1;
            for (unsigned face = 0; face < faces; ++face)
                REQUIRE(result->upload(*command, 0, face, {}, pixels, row, &error));
            REQUIRE(command->commitAndWait(&error));
            resources->textures.push_back(result);
            return result.get();
        };
        ALMetalTextureDescriptor d;
        d.type = ALMetalTextureType::texture_cube_array;
        d.width = d.height = d.array_length = 1;
        d.format = ALMetalPixelFormat::rgba16_float;
        const SHalf blue{ .125f, .375f, .75f, 1.f };
        probes.radiance_texture = make_texture(d, blue.data(), 8);
        d.type = ALMetalTextureType::texture_2d;
        d.width = 4; d.height = 1;
        d.format = ALMetalPixelFormat::rgba32_uint;
        std::array<std::uint32_t, 16> coefficients{};
        std::array<std::uint16_t, 4> packed;
        std::memcpy(packed.data(), blue.data(), sizeof(blue));
        coefficients[0] = packed[0] | (std::uint32_t(packed[1]) << 16);
        coefficients[1] = packed[2];
        probes.irradiance_texture = make_texture(d, coefficients.data(), 64);
        probes.constants.config[0] = 1.f;
        probes.constants.ref_sphere[0][3] = 100.f;
        probes.constants.ref_params[0][0] = probes.constants.ref_params[0][1] = probes.constants.ref_params[0][2] = 1.f;
        probes.constants.ref_params[0][3] = -100.f;
        probes.constants.ref_index[0][1] = -1;
        d.width = SW; d.height = SH; d.format = ALMetalPixelFormat::rg8_unorm;
        std::vector<std::uint8_t> lights(SW * SH * 2, 255);
        light_map = make_texture(d, lights.data(), SW * 2);
        // Deliberately asymmetric bright reflector: a narrow red ramp and a
        // shorter green horizontal marking. No temporal/SSR algorithm supplies
        // this authored texture or the geometry producing its history pixels.
        d.format = ALMetalPixelFormat::rgba16_float;
        std::vector<SHalf> pattern(SW * SH);
        for (unsigned y = 0; y < SH; ++y)
            for (unsigned x = 0; x < SW; ++x)
            {
                const float u = (x + .5f) / SW, v = (y + .5f) / SH;
                const float stripe = std::max(0.f, 1.f - std::abs(u - .36f) / .055f);
                const float upper = std::max(0.f, 1.f - std::abs(v - .73f) / .08f) * (u > .55f && u < .8f);
                pattern[y * SW + x] = SHalf{ __fp16(.05f + 4.f * stripe), __fp16(.04f + 2.f * upper), __fp16(.02f), __fp16(1.f) };
            }
        const auto* image = make_texture(d, pattern.data(), SW * 8);
        struct Geometry { float p[4][4], uv[4][2]; std::uint8_t color[4][4]; } geometry{};
        const float extent = .375f / cot;
        const float xy[4][2]{ {-extent,-extent}, {extent,-extent}, {extent,extent}, {-extent,extent} };
        for (unsigned i = 0; i < 4; ++i)
        {
            geometry.p[i][0] = xy[i][0]; geometry.p[i][1] = xy[i][1]; geometry.p[i][2] = -.375f; geometry.p[i][3] = 1.f;
            geometry.uv[i][0] = i == 1 || i == 2;
            geometry.uv[i][1] = i >= 2;
            std::fill_n(geometry.color[i], 4, 255);
        }
        const std::uint16_t indices[6]{0,1,2,0,2,3};
        landmark.vertices = buffer(&geometry, sizeof(geometry));
        landmark.indices = buffer(indices, sizeof(indices));
        landmark.vertex_count = 4; landmark.index_count = 6;
        landmark.texcoord_offset = offsetof(Geometry, uv);
        landmark.color_offset = offsetof(Geometry, color);
        landmark.normal_offset = sizeof(geometry) + 16; // Fullbright reads no normal stream.
        landmark.diffuse_textures[0] = image; landmark.texture_count = 1;
        landmark.impostor_fullbright = true;
        identity(landmark.modelview); identity(landmark.texture_matrix);
        receiver = makeReceiver();
    }
    float depth(float distance) const { return (-distance * projection[10] + projection[14]) / distance; }
    std::unique_ptr<ALMetalRenderTarget> makeReceiver()
    {
        ALMetalRenderTargetDescriptor d;
        d.width = SW; d.height = SH; d.color_attachment_count = 4;
        d.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        d.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        d.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        d.colors[3].format = ALMetalPixelFormat::rg11b10_float;
        d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        d.depth_stencil_shader_read = true;
        auto target = ALMetalRenderTarget::create(*device, d, &error);
        REQUIRE(target);
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {.25,.25,.25,0};
        clear.colors[1].clear = {1,1,1,1};
        clear.colors[2].clear = {1,1,1,.34};
        clear.clear_depth = depth(.5f);
        clear.depth_store = clear.stencil_store = ALMetalStoreAction::store;
        auto c = device->makeCommandBuffer("SSR receiver authored depth/material", &error);
        auto p = target->makeRenderPass(clear, &error);
        REQUIRE(c); REQUIRE(p);
        auto e = ALMetalRenderEncoder::begin(*c, *p, "SSR static receiver", &error);
        REQUIRE(e); REQUIRE(e->end(&error)); REQUIRE(c->commitAndWait(&error));
        return target;
    }
    std::array<float, 2> nextJitter()
    {
        ALMetalTemporalWorldHistory::FrameInput input;
        input.world_id = ++world;
        input.input_width = SW; input.input_height = SH;
        input.output_width = SW * 3 / 2; input.output_height = SH * 3 / 2;
        input.mode = 8; input.reverse_z = true; input.jitter_phase_count = 18;
        std::copy(projection.begin(), projection.end(), input.camera_mvp);
        REQUIRE(phase_history.beginFrame(input, &error));
        ALMetalWorldGBufferScene opaque; opaque.width = SW; opaque.height = SH;
        ALMetalWorldOpaquePostDeferredScene forward; forward.width = SW; forward.height = SH;
        REQUIRE(phase_history.stage(opaque, forward, &error));
        const auto jitter = phase_history.jitterPixels();
        REQUIRE(phase_history.commit(world, &error));
        return jitter;
    }
    ALMetalWorldSimpleBatch rowMarker()
    {
        struct Geometry { float p[4][4], uv[4][2]; std::uint8_t color[4][4]; } geometry{};
        constexpr float distance=.34f;
        const float clip[4][2]{{.2f,.35f},{.6f,.35f},{.6f,.72f},{.2f,.72f}};
        for(unsigned i=0;i<4;++i)
        {
            geometry.p[i][0]=clip[i][0]*distance/projection[0];
            geometry.p[i][1]=clip[i][1]*distance/projection[5];
            geometry.p[i][2]=-distance;geometry.p[i][3]=1.f;
            geometry.uv[i][0]=i==1||i==2;geometry.uv[i][1]=i>=2;
            std::fill_n(geometry.color[i],4,255);
        }
        auto marker=landmark;
        marker.vertices=buffer(&geometry,sizeof(geometry));
        const SHalf green{0.f,3.f,0.f,1.f};
        marker.diffuse_textures[0]=texture(ALMetalPixelFormat::rgba16_float,1,green.data(),8);
        return marker;
    }
    std::unique_ptr<ALMetalRenderTarget> source(const std::array<float, 16>& raster,bool row_marker=false)
    {
        data->beginFrame(++world);
        auto lighting = renderer->prepareTemporalTarget(SW, SH, false, &error); REQUIRE(lighting);
        ALMetalWorldSimpleBatch marker;
        if(row_marker)marker=rowMarker();
        REQUIRE(renderer->beginCommandBatch("SSR fixture ordinary source world",&error));
        ALMetalWorldGBufferScene empty;
        empty.width = SW; empty.height = SH; empty.hdr = true; empty.normal_hdr = true; empty.reverse_z = true;
        auto g = renderer->render(empty, &error); INFO(error); REQUIRE(g);
        REQUIRE(renderer->resolveTemporalColor(*g, *g, *lighting, &error));
        std::array<ALMetalWorldSimpleBatch,2> draws{landmark,marker};
        for(auto& draw:draws)std::copy(raster.begin(),raster.end(),draw.modelview_projection);
        ALMetalWorldOpaquePostDeferredScene scene;
        scene.gbuffer = g.get(); scene.composed_lighting = lighting.get();
        scene.batches = draws.data(); scene.batch_count = row_marker?2:1;
        scene.width = SW; scene.height = SH; scene.hdr = true; scene.reverse_z = true;
        scene.publish_screen_space_reflection_history = true;
        auto result = renderer->renderOpaquePostDeferred(scene, &error); INFO(error); REQUIRE(result);
        const auto completion=renderer->commandBatchCompletion();REQUIRE(completion);
        REQUIRE(renderer->commitCommandBatch(&error));REQUIRE(completion->waitUntilCompleted(&error));
        return result;
    }
    void rasterReceiver(const std::array<float,16>& raster)
    {
        struct MirrorGeometry
        {
            float p[4][4]{},n[4][4]{},uv[4][2]{},tangent[4][4]{};
            std::uint8_t color[4][4]{};
        } geometry;
        const float extent=.6f/projection[0];
        const float corners[4][2]{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}};
        for(unsigned i=0;i<4;++i)
        {
            geometry.p[i][0]=corners[i][0];geometry.p[i][1]=corners[i][1];geometry.p[i][2]=-.5f;geometry.p[i][3]=1;
            geometry.n[i][2]=-1;
            geometry.tangent[i][0]=geometry.tangent[i][3]=1;
            geometry.uv[i][0]=i==1||i==2;geometry.uv[i][1]=i>=2;
            std::fill_n(geometry.color[i],4,255);
        }
        auto value=batch(1);
        const std::uint16_t index[6]{0,1,2,0,2,3};
        value.vertices=buffer(&geometry,sizeof(geometry));value.indices=buffer(index,sizeof(index));
        value.vertex_count=4;value.index_count=6;
        value.normal_offset=offsetof(MirrorGeometry,n);value.texcoord_offset=offsetof(MirrorGeometry,uv);
        value.tangent_offset=offsetof(MirrorGeometry,tangent);value.color_offset=offsetof(MirrorGeometry,color);
        value.weight_offset=value.cloth_offset=0;
        const std::uint8_t white[4]{255,255,255,255};
        const auto* image=texture(ALMetalPixelFormat::rgba8_unorm,1,white,4);
        value.diffuse_textures[0]=image;
        auto& material=value.materialSlots()[0];material.specular_texture=image;
        std::fill_n(material.specular_color,4,1.f);material.env_intensity=1.f;
        std::copy(raster.begin(),raster.end(),value.modelview_projection);
        identity(value.modelview);identity(value.texture_matrix);
        auto g=scene();g.width=SW;g.height=SH;g.hdr=g.normal_hdr=g.reverse_z=true;
        g.batches=&value;g.batch_count=1;
        receiver=renderer->render(g,&error);INFO(error);REQUIRE(receiver);
    }
    ALMetalWorldScreenSpaceReflections reflections(const ALMetalRenderTarget& history, const std::array<float,16>& raster)
    {
        ALMetalWorldScreenSpaceReflections r;
        r.scene_color = history.colorTexture(0); r.scene_depth = history.colorTexture(1);
        const auto inv = ssrInverse(raster);
        std::copy(raster.begin(), raster.end(), r.constants.projection);
        std::copy(inv.begin(), inv.end(), r.constants.inverse_projection);
        identity(r.constants.inverse_modelview_delta);
        r.constants.screen[0] = SW; r.constants.screen[1] = r.constants.screen[2] = SH;
        r.constants.trace[0] = 4; r.constants.trace[1] = .1f; r.constants.trace[2] = .2f; r.constants.trace[3] = .001f;
        r.constants.sampling[0] = 1; r.constants.sampling[1] = 1.6f;
        r.constants.config[0] = r.constants.config[1] = 1;
        return r;
    }
#if ALCHEMY_SSR_COORDINATE_HISTORY
    ALMetalWorldScreenSpaceReflectionCoordinates coordinates(const std::array<float,16>& raster) const
    {
        ALMetalWorldScreenSpaceReflectionCoordinates c;
        std::copy(raster.begin(),raster.end(),c.projection);
        const auto inv=ssrInverse(raster);
        std::copy(inv.begin(),inv.end(),c.inverse_projection);
        identity(c.modelview);
        c.camera_generation=world;
        c.source_world=world;
        c.reverse_z=true;
        return c;
    }
#endif
    void publish(const ALMetalRenderTarget& source,const std::array<float,16>& raster)
    {
#if ALCHEMY_SSR_COORDINATE_HISTORY
        REQUIRE(renderer->updateScreenSpaceReflectionHistory(source,true,coordinates(raster),typed_history,&error));
#else
        static_cast<void>(raster);
        REQUIRE(renderer->updateScreenSpaceReflectionHistory(source,true,raw_history,&error));
#endif
    }
    const ALMetalRenderTarget& published() const
    {
#if ALCHEMY_SSR_COORDINATE_HISTORY
        REQUIRE(typed_history.valid()); REQUIRE(typed_history.target());
        return *typed_history.target();
#else
        REQUIRE(raw_history); return *raw_history;
#endif
    }
    ALMetalWorldScreenSpaceReflections actualReflections()
    {
        auto result=reflections(published(),projection);
#if ALCHEMY_SSR_COORDINATE_HISTORY
        std::array<float,16> current_inverse{}; identity(current_inverse.data());
        const double origin[3]{};
        REQUIRE(typed_history.bind(result,current_inverse.data(),origin,&error));
#endif
        return result;
    }
    std::vector<SColor> consume(const ALMetalWorldScreenSpaceReflections* reflections,
                                const std::array<float,16>* current_raster=nullptr)
    {
        ALMetalWorldBaseLightingScene scene;
        scene.gbuffer = receiver.get(); scene.light_map = light_map; scene.reflection_probes = &probes;
        scene.screen_space_reflections = reflections; scene.hdr = true;
        const auto current_inverse=current_raster?ssrInverse(*current_raster):inverse;
        std::copy(current_inverse.begin(),current_inverse.end(),scene.constants.inverse_projection);
        scene.constants.config[0] = 1; scene.constants.light_direction[2] = 1.f; scene.constants.surface[1] = 1.f;
        auto target = renderer->renderBaseLighting(scene, &error); INFO(error); REQUIRE(target);
        std::vector<SHalf> values(SW * SH);
        REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},values.data(),SW*8,&error));
        std::vector<SColor> result(values.size());
        for (unsigned i=0;i<values.size();++i)for(unsigned c=0;c<4;++c)result[i][c]=values[i][c];
        return result;
    }
};
double ssrDifference(const std::vector<SColor>& a, const std::vector<SColor>& b)
{
    REQUIRE(a.size() == SW*SH); REQUIRE(b.size() == a.size());
    double error=0;
    for(unsigned y=4;y<SH-4;++y)for(unsigned x=4;x<SW-4;++x)for(unsigned c=0;c<3;++c)
        error += std::abs(a[y*SW+x][c]-b[y*SW+x][c]);
    return error / ((SW-8)*(SH-8)*3);
}
void ssrSave(const std::filesystem::path& path,const std::vector<SColor>& data)
{
    std::ofstream out(path,std::ios::binary); REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(data.data()),data.size()*sizeof(SColor)); REQUIRE(out.good());
}
}

TEST_CASE("SSR sampling follows the jittered history raster rather than the current projection",
          "[metal][ssr-temporal-coordinates]")
{
    SsrRasterFixture f;
    const auto probe=f.consume(nullptr);
    double actual_oracle_error=0,legacy_oracle_error=0;
    std::vector<std::array<double,5>> metrics;
    for(unsigned phase=0;phase<18;++phase)
    {
        CAPTURE(phase);
        const auto jitter=f.nextJitter();
        auto raster=f.projection;
        alMetalApplyTemporalJitter(raster.data(),jitter[0],jitter[1],SW,SH);
        auto source=f.source(raster);
        f.publish(*source,raster);
        auto oracle=f.reflections(f.published(),raster);
        auto legacy=f.reflections(f.published(),f.projection);
        const auto correct=f.consume(&oracle),old=f.consume(&legacy);
        // Source-defined baseline: the old viewer always used the current
        // unjittered camera projection, independently of retained history.
        auto actual_pack=f.actualReflections();
        const auto actual=f.consume(&actual_pack);
        const auto actual_error=ssrDifference(actual,correct),legacy_error=ssrDifference(old,correct);
        actual_oracle_error+=actual_error/18;
        legacy_oracle_error+=legacy_error/18;
        metrics.push_back({double(phase),jitter[0],jitter[1],actual_error,legacy_error});
        REQUIRE(ssrDifference(correct,probe)>1.e-5); // A no-hit/empty fixture cannot qualify.
        if(const char* p=std::getenv("ALCHEMY_SSR_COORDINATE_OUTPUT"))
        {
            const auto dir=std::filesystem::path(p);std::filesystem::create_directories(dir);
            ssrSave(dir/("world-"+std::to_string(phase)+"-actual.rgba32f"),actual);
            ssrSave(dir/("world-"+std::to_string(phase)+"-oracle.rgba32f"),correct);
            ssrSave(dir/("world-"+std::to_string(phase)+"-legacy.rgba32f"),old);
        }
    }
    if(const char* p=std::getenv("ALCHEMY_SSR_COORDINATE_OUTPUT"))
    {
        std::ofstream out(std::filesystem::path(p)/"metrics.json");REQUIRE(out.good());
        out<<std::setprecision(17)<<"{\"input_width\":"<<SW<<",\"input_height\":"<<SH
           <<",\"typed_history\":"<<(ALCHEMY_SSR_COORDINATE_HISTORY?"true":"false")
           <<",\"actual_oracle_rgb_mae\":"<<actual_oracle_error<<",\"legacy_oracle_rgb_mae\":"<<legacy_oracle_error
           <<",\"rows\":[";
        for(unsigned i=0;i<metrics.size();++i)
        {
            if(i)out<<',';
            const auto& v=metrics[i];out<<"{\"phase\":"<<v[0]<<",\"jitter_input_pixels\":["<<v[1]<<','<<v[2]
               <<"],\"actual_oracle_rgb_mae\":"<<v[3]<<",\"legacy_oracle_rgb_mae\":"<<v[4]<<'}';
        }
        out<<"]}\n";REQUIRE(out.good());
    }
    INFO("actual/oracle RGB MAE="<<actual_oracle_error<<" legacy/oracle="<<legacy_oracle_error);
    CHECK(legacy_oracle_error>1.e-6); // Prove this fixture distinguishes the wrong coordinate generation.
    CHECK(actual_oracle_error==0); // Correct publisher/consumer pairing must match the independently known raster.
}

TEST_CASE("Jittered SSR publication preserves native asymmetric color and depth rows",
          "[metal][ssr-temporal-coordinates][ssr-temporal-rows]")
{
    SsrRasterFixture f;
    auto raster=f.projection;
    const auto jitter=f.nextJitter();
    alMetalApplyTemporalJitter(raster.data(),jitter[0],jitter[1],SW,SH);
    auto source=f.source(raster,true);f.publish(*source,raster);
    std::vector<SHalf> original(SW*SH);
    std::vector<float> original_depth(SW*SH),history_depth(SW*SH);
    std::vector<std::uint32_t> color(SW*SH);
    REQUIRE(source->colorTexture(0)->readback(*f.device,0,0,{},original.data(),SW*8,&f.error));
    REQUIRE(source->depthStencilTexture()->readback(*f.device,0,0,{},original_depth.data(),SW*4,&f.error));
    REQUIRE(f.published().colorTexture(0)->readback(*f.device,0,0,{},color.data(),SW*4,&f.error));
    REQUIRE(f.published().colorTexture(1)->readback(*f.device,0,0,{},history_depth.data(),SW*4,&f.error));
    const auto unpack=[](std::uint32_t bits,unsigned mantissa)
    {
        const unsigned exponent=bits>>mantissa, fraction=bits&((1u<<mantissa)-1u);
        return exponent==0 ? std::ldexp(float(fraction),-14-int(mantissa)) :
            std::ldexp(1.f+float(fraction)/float(1u<<mantissa),int(exponent)-15);
    };
    unsigned wrong_color=0,wrong_depth=0,flipped_depth_mismatches=0,near_pixels=0;
    double near_y=0;
    double red_mass=0,red_x=0,green_mass=0,green_y=0;
    for(unsigned y=0;y<SH;++y)for(unsigned x=0;x<SW;++x)
    {
        const auto i=y*SW+x;
        wrong_depth+=std::abs(original_depth[i]-history_depth[i])>1.e-7f;
        flipped_depth_mismatches+=std::abs(original_depth[i]-history_depth[(SH-1-y)*SW+x])>1.e-7f;
        if(std::abs(original_depth[i]-f.depth(.34f))<1.e-6f){++near_pixels;near_y+=y;}
        for(unsigned channel=0;channel<3;++channel)
        {
            const float actual=unpack((color[i]>>(channel==2?22:channel*11))&(channel==2?0x3ff:0x7ff),channel==2?5:6);
            const float expected=original[i][channel];
            // R11/G11/B10 quantization, not an image-quality relaxation.
            wrong_color+=!std::isfinite(actual)||std::abs(actual-expected)>.04f*std::max(1.f,std::abs(expected));
            if(channel==0){red_mass+=actual;red_x+=x*actual;}
            if(channel==1){green_mass+=actual;green_y+=y*actual;}
        }
    }
    REQUIRE(red_mass>10);REQUIRE(green_mass>10);
    CHECK(red_x/red_mass<SW*.5); // Red ramp is left of center.
    CHECK(green_y/green_mass>SH*.6); // Green marking occupies the authored positive-Y rows.
    REQUIRE(near_pixels>100);CHECK(near_y/near_pixels>SH*.6);
    CHECK(flipped_depth_mismatches>100); // Negative control: a depth-only vertical flip must be detected.
    CHECK(wrong_color==0);CHECK(wrong_depth==0);
}

TEST_CASE("Current jittered material raster consumes the separately published SSR raster",
          "[metal][ssr-temporal-coordinates][ssr-temporal-current-raster]")
{
    SsrRasterFixture f;
    auto stored=f.projection;
    auto first_jitter=f.nextJitter();
    alMetalApplyTemporalJitter(stored.data(),first_jitter[0],first_jitter[1],SW,SH);
    auto previous=f.source(stored);f.publish(*previous,stored);
    double actual_error=0,legacy_error=0;
    for(unsigned phase=0;phase<18;++phase)
    {
        CAPTURE(phase);
        const auto jitter=f.nextJitter();
        auto current=f.projection;
        alMetalApplyTemporalJitter(current.data(),jitter[0],jitter[1],SW,SH);
        // This receiver is actual material geometry rasterized with current,
        // not a cleared G-buffer reinterpreted using an invented projection.
        f.rasterReceiver(current);
        auto actual_pack=f.actualReflections(),oracle_pack=f.reflections(f.published(),stored),legacy_pack=f.reflections(f.published(),f.projection);
        const auto actual=f.consume(&actual_pack,&current),oracle=f.consume(&oracle_pack,&current),legacy=f.consume(&legacy_pack,&current);
        actual_error+=ssrDifference(actual,oracle)/18;
        legacy_error+=ssrDifference(legacy,oracle)/18;
        const auto probe=f.consume(nullptr,&current);
        REQUIRE(ssrDifference(oracle,probe)>1.e-5);
        if(const char* p=std::getenv("ALCHEMY_SSR_COORDINATE_OUTPUT"))
        {
            const auto dir=std::filesystem::path(p)/"current-raster";std::filesystem::create_directories(dir);
            ssrSave(dir/("world-"+std::to_string(phase)+"-actual.rgba32f"),actual);
            ssrSave(dir/("world-"+std::to_string(phase)+"-oracle.rgba32f"),oracle);
            ssrSave(dir/("world-"+std::to_string(phase)+"-legacy.rgba32f"),legacy);
        }
        auto next=f.source(current);f.publish(*next,current);stored=current;
    }
    INFO("current-raster actual/oracle MAE="<<actual_error<<" legacy/oracle="<<legacy_error);
    CHECK(legacy_error>1.e-6);
    CHECK(actual_error==0);
}

#if ALCHEMY_SSR_COORDINATE_HISTORY
TEST_CASE("SSR coordinates remain paired across skipped rejected and recovered publications",
          "[metal][ssr-temporal-coordinates][ssr-temporal-publication]")
{
    SsrRasterFixture f;
    auto first_raster=f.projection;
    const auto first_jitter=f.nextJitter();
    alMetalApplyTemporalJitter(first_raster.data(),first_jitter[0],first_jitter[1],SW,SH);
    auto first=f.source(first_raster);f.publish(*first,first_raster);
    const auto generation=f.typed_history.generation();
    const auto* target=f.typed_history.target();
    REQUIRE(f.typed_history.coordinates());
    const auto snapshot=*f.typed_history.coordinates();
    auto initial=f.actualReflections();const auto expected=f.consume(&initial);
    auto second_raster=f.projection;
    const auto second_jitter=f.nextJitter();
    alMetalApplyTemporalJitter(second_raster.data(),second_jitter[0],second_jitter[1],SW,SH);
    auto second=f.source(second_raster);
    REQUIRE(second_raster!=first_raster);
    // A completed world need not publish SSR. Advancing its jitter/camera must
    // not silently reinterpret the preceding generation's color and depth.
    auto skipped=f.actualReflections();CHECK(f.consume(&skipped)==expected);
    CHECK(std::equal(first_raster.begin(),first_raster.end(),skipped.constants.projection));
    auto invalid=f.renderer->prepareTemporalTarget(SW,SH,false,&f.error);REQUIRE(invalid);
    CHECK_FALSE(f.renderer->updateScreenSpaceReflectionHistory(*invalid,true,f.coordinates(second_raster),f.typed_history,&f.error));
    auto bad=f.coordinates(second_raster);bad.inverse_projection[0]=std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(f.renderer->updateScreenSpaceReflectionHistory(*second,true,bad,f.typed_history,&f.error));
    auto zero=f.coordinates(second_raster);
    std::fill_n(zero.projection,16,0.f);std::fill_n(zero.inverse_projection,16,0.f);
    CHECK_FALSE(f.renderer->updateScreenSpaceReflectionHistory(*second,true,zero,f.typed_history,&f.error));
    auto mismatched=f.coordinates(second_raster);mismatched.inverse_projection[0]*=1.25f;
    CHECK_FALSE(f.renderer->updateScreenSpaceReflectionHistory(*second,true,mismatched,f.typed_history,&f.error));
    auto singular_view=f.coordinates(second_raster);std::fill_n(singular_view.modelview,16,0.f);
    CHECK_FALSE(f.renderer->updateScreenSpaceReflectionHistory(*second,true,singular_view,f.typed_history,&f.error));
    CHECK(f.typed_history.generation()==generation);CHECK(f.typed_history.target()==target);
    REQUIRE(f.typed_history.coordinates());
    const auto& retained=*f.typed_history.coordinates();
    CHECK(std::equal(std::begin(snapshot.projection),std::end(snapshot.projection),retained.projection));
    CHECK(std::equal(std::begin(snapshot.inverse_projection),std::end(snapshot.inverse_projection),retained.inverse_projection));
    CHECK(std::equal(std::begin(snapshot.modelview),std::end(snapshot.modelview),retained.modelview));
    CHECK(std::equal(std::begin(snapshot.world_origin),std::end(snapshot.world_origin),retained.world_origin));
    CHECK(retained.source_world==snapshot.source_world);CHECK(retained.camera_generation==snapshot.camera_generation);
    auto after_failure=f.actualReflections();CHECK(f.consume(&after_failure)==expected);

    auto next=f.coordinates(second_raster);
    constexpr float angle=.3f,tilt=.2f;
    identity(next.modelview);
    next.modelview[0]=next.modelview[5]=std::cos(angle);
    next.modelview[1]=std::sin(angle);next.modelview[4]=-std::sin(angle);
    next.modelview[12]=.11f;next.modelview[13]=.07f;
    REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*second,true,next,f.typed_history,&f.error));
    CHECK(f.typed_history.generation()==generation+1);
    std::array<float,16> current_inverse{};identity(current_inverse.data());
    current_inverse[5]=current_inverse[10]=std::cos(tilt);
    current_inverse[6]=std::sin(tilt);current_inverse[9]=-std::sin(tilt);
    current_inverse[13]=-.3f;current_inverse[14]=.2f;
    const double origin[3]{};
    auto pack=f.reflections(f.published(),f.projection);
    const std::array<float,16> singular_current{};
    CHECK_FALSE(f.typed_history.bind(pack,singular_current.data(),origin,&f.error));
    CHECK(pack.scene_color==nullptr);CHECK(pack.constants.config[1]==0);
    REQUIRE(f.typed_history.bind(pack,current_inverse.data(),origin,&f.error));
    for(const auto& point:std::array<std::array<float,3>,3>{{{.2f,.3f,-.5f},{-.7f,.1f,-2.f},{0.f,-.4f,-1.f}}})
    {
        // Independent pointwise application of two noncommuting transforms.
        // A translation-only test would not catch reversed matrix order.
        const float qx=point[0],qy=std::cos(tilt)*point[1]-std::sin(tilt)*point[2]-.3f,
                    qz=std::sin(tilt)*point[1]+std::cos(tilt)*point[2]+.2f;
        const std::array<float,3> oracle{std::cos(angle)*qx-std::sin(angle)*qy+.11f,
            std::sin(angle)*qx+std::cos(angle)*qy+.07f,qz};
        for(unsigned row=0;row<3;++row)
        {
            const auto* m=pack.constants.inverse_modelview_delta;
            const float actual=m[row]*point[0]+m[4+row]*point[1]+m[8+row]*point[2]+m[12+row];
            CHECK(std::abs(actual-oracle[row])<3.e-7f);
        }
    }
    const double changed_origin[3]{256,0,0};
    CHECK_FALSE(f.typed_history.bind(pack,current_inverse.data(),changed_origin,&f.error));
    CHECK(pack.scene_color==nullptr);CHECK(pack.scene_depth==nullptr);CHECK(pack.constants.config[1]==0);
    REQUIRE(f.typed_history.valid());
    std::copy_n(changed_origin,3,next.world_origin);
    REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*second,true,next,f.typed_history,&f.error));
    REQUIRE(f.typed_history.bind(pack,current_inverse.data(),changed_origin,&f.error));
    CHECK(pack.scene_color==f.typed_history.target()->colorTexture(0));

    // A real independently allocated/raster-cleared smaller view, rather than
    // resized saved input pixels, exercises descriptor replacement and recovery.
    ALMetalRenderTargetDescriptor smaller_descriptor;
    smaller_descriptor.width=SW/2;smaller_descriptor.height=SH/2;
    smaller_descriptor.colors[0].format=ALMetalPixelFormat::rgba16_float;
    smaller_descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
    smaller_descriptor.depth_stencil_shader_read=true;
    auto smaller=ALMetalRenderTarget::create(*f.device,smaller_descriptor,&f.error);REQUIRE(smaller);
    ALMetalRenderPassConfig clear;
    clear.colors[0].clear={.8,.1,.3,.25};clear.clear_depth=f.depth(.375f);
    clear.depth_store=clear.stencil_store=ALMetalStoreAction::store;
    auto pass=smaller->makeRenderPass(clear,&f.error);REQUIRE(pass);
    auto command=f.device->makeCommandBuffer("SSR actual half-size resize source",&f.error);REQUIRE(command);
    auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"SSR resized color and depth clear",&f.error);REQUIRE(encoder);
    REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error)); // Offline fixture boundary.
    auto resized=f.coordinates(f.projection);
    std::copy_n(changed_origin,3,resized.world_origin);
    resized.source_world=next.source_world+100;
    resized.camera_generation=next.camera_generation+1;
    const auto before_resize=f.typed_history.generation();
    REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*smaller,true,resized,f.typed_history,&f.error));
    CHECK(f.typed_history.generation()==before_resize+1);
    REQUIRE(f.typed_history.coordinates());
    CHECK(f.typed_history.coordinates()->source_world==resized.source_world);
    REQUIRE(f.typed_history.bind(pack,current_inverse.data(),changed_origin,&f.error));
    CHECK(pack.constants.screen[0]==SW/2);CHECK(pack.constants.screen[1]==SH/2);CHECK(pack.constants.screen[2]==SH/2);
    CHECK(pack.scene_color->width()==SW/2);CHECK(pack.scene_depth->height()==SH/2);
    const auto before_reset=f.typed_history.generation();
    f.typed_history.reset();CHECK_FALSE(f.typed_history.valid());CHECK(f.typed_history.coordinates()==nullptr);
    CHECK(f.typed_history.target()==nullptr);CHECK(f.typed_history.generation()==0);
    ++resized.source_world;++resized.camera_generation;
    REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*smaller,true,resized,f.typed_history,&f.error));
    REQUIRE(f.typed_history.bind(pack,current_inverse.data(),changed_origin,&f.error));
    // Invalid history exposes0, but reset preserves the internal serial so a
    // newly published generation cannot reuse an earlier successful identity.
    CHECK(f.typed_history.generation()==before_reset+1);
    REQUIRE(f.typed_history.coordinates());CHECK(f.typed_history.coordinates()->source_world==resized.source_world);
    CHECK(pack.constants.screen[0]==SW/2);CHECK(pack.constants.screen[1]==SH/2);
}
#endif
