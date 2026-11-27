/**
 * @file almetal_tile_sampling_test.mm
 * @brief Raw-HDR rough-SSR characterization, independent of reconstruction/effects.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authored camera/bright-history primitives adapt the paired-publication fixture
 * in almetal_ssr_temporal_coordinates_test.mm. This is not an owner-scene replay.
 * A real constant-material PBR draw excludes normal/ORM texture detail as the
 * source of measured variation. The high-budget arm uses the same stochastic
 * SSR shader, not a physically exact or independent reflection oracle.
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetaltemporalworldhistory.h"
#include <simd/simd.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace
{
constexpr unsigned TW = 96, TH = 64, TP = 18;
using TilePixel = std::array<float,4>;
using TileHalf = std::array<__fp16,4>;
using TileImage = std::vector<TilePixel>;

std::array<float,16> tileInverse(const std::array<float,16>& value)
{
    simd_float4x4 matrix; std::memcpy(&matrix,value.data(),sizeof(matrix));
    const auto inverse=simd_inverse(matrix);
    std::array<float,16> result; std::memcpy(result.data(),&inverse,sizeof(inverse));
    return result;
}

struct TileSsrFixture : Fixture
{
    std::array<float,16> projection{}, inverse{};
    ALMetalWorldReflectionProbes probes;
    ALMetalWorldScreenSpaceReflectionHistory history;
    const ALMetalTexture* light_map=nullptr;
    unsigned data_world=0;

    const ALMetalTexture* image(ALMetalTextureDescriptor d,const void* bytes,unsigned row)
    {
        d.usage=ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> value=ALMetalTexture::create(*device,d,&error);
        INFO(error); REQUIRE(value);
        auto command=device->makeCommandBuffer("Tile characterization authored texture",&error); REQUIRE(command);
        const unsigned faces=d.type==ALMetalTextureType::texture_cube_array ? 6u:1u;
        for(unsigned face=0;face<faces;++face)
            REQUIRE(value->upload(*command,0,face,{},bytes,row,&error));
        REQUIRE(command->commitAndWait(&error)); resources->textures.push_back(value);
        return value.get();
    }

    TileSsrFixture()
    {
        const float near=.1f,far=10.f,cot=1.f/std::tan(.5f);
        projection[0]=projection[5]=cot; projection[10]=near/(far-near);
        projection[11]=-1.f; projection[14]=far*near/(far-near);
        inverse=tileInverse(projection);
        ALMetalTextureDescriptor d;
        d.type=ALMetalTextureType::texture_cube_array;
        d.width=d.height=d.array_length=1; d.format=ALMetalPixelFormat::rgba16_float;
        const TileHalf blue{.125f,.375f,.75f,1.f};
        probes.radiance_texture=image(d,blue.data(),8);
        d.type=ALMetalTextureType::texture_2d;d.width=4;d.height=1;
        d.format=ALMetalPixelFormat::rgba32_uint;
        std::array<std::uint32_t,16> coefficients{};
        std::array<std::uint16_t,4> packed;std::memcpy(packed.data(),blue.data(),sizeof(blue));
        coefficients[0]=packed[0]|(std::uint32_t(packed[1])<<16);coefficients[1]=packed[2];
        probes.irradiance_texture=image(d,coefficients.data(),64);
        probes.constants.config[0]=1.f;probes.constants.ref_sphere[0][3]=100.f;
        probes.constants.ref_params[0][0]=probes.constants.ref_params[0][1]=probes.constants.ref_params[0][2]=1.f;
        probes.constants.ref_params[0][3]=-100.f;probes.constants.ref_index[0][1]=-1;
        d.width=TW;d.height=TH;d.format=ALMetalPixelFormat::rg8_unorm;
        const std::vector<std::uint8_t> lit(TW*TH*2,255);
        light_map=image(d,lit.data(),TW*2);
        publishBrightHistory();
    }

    void publishBrightHistory()
    {
        ALMetalTextureDescriptor d;d.width=TW;d.height=TH;d.format=ALMetalPixelFormat::rgba16_float;
        std::vector<TileHalf> pattern(TW*TH);
        for(unsigned y=0;y<TH;++y)for(unsigned x=0;x<TW;++x)
        {
            const float u=(x+.5f)/TW,v=(y+.5f)/TH;
            const float stripe=std::max(0.f,1.f-std::abs(u-.36f)/.055f);
            const float short_stripe=std::max(0.f,1.f-std::abs(v-.73f)/.08f)*(u>.55f&&u<.8f);
            pattern[y*TW+x]={__fp16(.05f+4.f*stripe),__fp16(.04f+2.f*short_stripe),__fp16(.02f),__fp16(1.f)};
        }
        const auto* texture=image(d,pattern.data(),TW*8);
        struct Card {float p[4][4],uv[4][2];std::uint8_t color[4][4];} card{};
        // The receiver is a front-facing grazing floor. Its reflected rays
        // travel farther into the view, so their visible history plane must
        // be behind the receiver, rather than the legacy fixture's near card.
        constexpr float distance=1.3f;
        const float extent=distance/projection[0];
        const float corners[4][2]{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}};
        for(unsigned i=0;i<4;++i)
        {
            card.p[i][0]=corners[i][0];card.p[i][1]=corners[i][1];card.p[i][2]=-distance;card.p[i][3]=1.f;
            card.uv[i][0]=i==1||i==2;card.uv[i][1]=i>=2;std::fill_n(card.color[i],4,255);
        }
        const std::uint16_t index[6]{0,1,2,0,2,3};
        ALMetalWorldSimpleBatch draw;draw.vertices=buffer(&card,sizeof(card));draw.indices=buffer(index,sizeof(index));
        draw.vertex_count=4;draw.index_count=6;draw.texcoord_offset=offsetof(Card,uv);
        draw.color_offset=offsetof(Card,color);draw.normal_offset=sizeof(card)+16;
        draw.texture_count=1;draw.diffuse_textures[0]=texture;draw.impostor_fullbright=true;
        std::copy(projection.begin(),projection.end(),draw.modelview_projection);
        identity(draw.modelview);identity(draw.texture_matrix);
        data->beginFrame(++data_world);
        auto lighting=renderer->prepareTemporalTarget(TW,TH,false,&error);REQUIRE(lighting);
        REQUIRE(renderer->beginCommandBatch("Tile characterization real bright source",&error));
        auto empty=scene();empty.width=TW;empty.height=TH;empty.hdr=empty.normal_hdr=empty.reverse_z=true;
        auto g=renderer->render(empty,&error);INFO(error);REQUIRE(g);
        REQUIRE(renderer->resolveTemporalColor(*g,*g,*lighting,&error));
        ALMetalWorldOpaquePostDeferredScene forward;
        forward.gbuffer=g.get();forward.composed_lighting=lighting.get();forward.batches=&draw;forward.batch_count=1;
        forward.width=TW;forward.height=TH;forward.hdr=forward.reverse_z=true;
        forward.publish_screen_space_reflection_history=true;
        auto source=renderer->renderOpaquePostDeferred(forward,&error);INFO(error);REQUIRE(source);
        const auto completion=renderer->commandBatchCompletion();REQUIRE(completion);
        REQUIRE(renderer->commitCommandBatch(&error));REQUIRE(completion->waitUntilCompleted(&error));
        ALMetalWorldScreenSpaceReflectionCoordinates coordinates;
        std::copy(projection.begin(),projection.end(),coordinates.projection);
        std::copy(inverse.begin(),inverse.end(),coordinates.inverse_projection);
        identity(coordinates.modelview);coordinates.camera_generation=coordinates.source_world=1;coordinates.reverse_z=true;
        REQUIRE(renderer->updateScreenSpaceReflectionHistory(*source,true,coordinates,history,&error));
        REQUIRE(history.valid());
    }

    ALMetalWorldSimpleBatch receiverBatch(float roughness,float angle)
    {
        auto geometry=this->geometry;
        const float s=std::sin(angle),c=std::cos(angle);
        // Intersect authored screen-corner rays with n·(p-(0,0,-1))=0.
        // This is genuine perspective floor geometry, not a normal-only tilt.
        // The measured interior lies inside its common footprint at both
        // angles; limiting its upper extent avoids the plane's horizon.
        const float xy[4][2]{{-1.05f,-.72f},{1.05f,-.72f},{1.05f,.72f},{-1.05f,.72f}};
        for(unsigned i=0;i<4;++i)
        {
            const float distance=c/(c-s*xy[i][1]/projection[5]);
            REQUIRE(distance>.1f);REQUIRE(distance<10.f);
            geometry.positions[i][0]=xy[i][0]*distance/projection[0];
            geometry.positions[i][1]=xy[i][1]*distance/projection[5];
            geometry.positions[i][2]=-distance;geometry.positions[i][3]=1.f;
            geometry.normals[i][0]=0.f;geometry.normals[i][1]=s;geometry.normals[i][2]=c;
            geometry.tangents[i][0]=1.f;geometry.tangents[i][1]=geometry.tangents[i][2]=0.f;geometry.tangents[i][3]=1.f;
            geometry.uv[i][0]=i==1||i==2;geometry.uv[i][1]=i>=2;std::fill_n(geometry.colors[i],4,255);
        }
        const std::uint16_t index[6]{0,1,2,0,2,3};
        auto draw=batch(2);draw.vertices=buffer(&geometry,sizeof(geometry));draw.indices=buffer(index,sizeof(index));
        draw.index_count=6;identity(draw.texture_matrix);
        const std::uint8_t gray[4]{180,180,180,255},white_orm[4]{255,255,255,255};
        draw.diffuse_textures[0]=texture(ALMetalPixelFormat::rgba8_unorm_srgb,1,gray,4);
        auto& material=draw.pbrSlots()[0];material.orm_texture=texture(ALMetalPixelFormat::rgba8_unorm,1,white_orm,4);
        material.roughness_factor=roughness;material.metallic_factor=1.f;
        std::fill_n(material.emissive_color,4,0.f);
        return draw;
    }

    std::unique_ptr<ALMetalRenderTarget> receiver(ALMetalWorldSimpleBatch draw,const std::array<float,16>& raster)
    {
        std::copy(raster.begin(),raster.end(),draw.modelview_projection);
        auto g=scene();g.width=TW;g.height=TH;g.hdr=g.normal_hdr=g.reverse_z=true;
        g.material_mip_bias=std::log2(2.f/3.f)-1.f; // Constant 1x1 maps deliberately isolate SSR.
        g.batches=&draw;g.batch_count=1;
        auto target=renderer->render(g,&error);INFO(error);REQUIRE(target);return target;
    }

    void checkReceiver(const ALMetalRenderTarget& target,float roughness)
    {
        std::vector<std::array<std::uint16_t,4>> normals(TW*TH);
        std::vector<std::array<std::uint8_t,4>> orm(TW*TH);
        REQUIRE(target.colorTexture(2)->readback(*device,0,0,{},normals.data(),TW*8,&error));
        REQUIRE(target.colorTexture(1)->readback(*device,0,0,{},orm.data(),TW*4,&error));
        // Establish that every measured pixel is the authored PBR surface,
        // rather than relying on a nonempty target or finite BASE output.
        for(unsigned y=12;y<TH-12;++y)for(unsigned x=12;x<TW-12;++x)
            REQUIRE(std::abs(normals[y*TW+x][3]/65535.f-.67f)<.01f);
        const auto& center=orm[(TH/2)*TW+TW/2];
        const float actual_roughness=center[1]/255.f+center[3]/(255.f*255.f);
        REQUIRE(std::abs(actual_roughness-roughness)<1.f/255.f);
    }

    TileImage consume(const ALMetalRenderTarget& g,const std::array<float,16>& raster,unsigned budget)
    {
        ALMetalWorldScreenSpaceReflections ssr;
        ssr.constants.trace[0]=25.f;ssr.constants.trace[1]=.1f;
        ssr.constants.trace[2]=.04f;ssr.constants.trace[3]=.001f;
        ssr.constants.sampling[0]=float(budget);ssr.constants.sampling[1]=1.6f;
        std::array<float,16> camera_inverse{};identity(camera_inverse.data());const double origin[3]{};
        REQUIRE(history.bind(ssr,camera_inverse.data(),origin,&error));
        ALMetalWorldBaseLightingScene base;base.gbuffer=&g;base.light_map=light_map;
        base.reflection_probes=&probes;base.hdr=true;base.screen_space_reflections=budget?&ssr:nullptr;
        const auto current_inverse=tileInverse(raster);
        std::copy(current_inverse.begin(),current_inverse.end(),base.constants.inverse_projection);
        base.constants.config[0]=1;base.constants.light_direction[2]=1.f;base.constants.surface[1]=1.f;
        auto output=renderer->renderBaseLighting(base,&error);INFO(error);REQUIRE(output);
        std::vector<TileHalf> halves(TW*TH);
        REQUIRE(output->colorTexture(0)->readback(*device,0,0,{},halves.data(),TW*8,&error));
        TileImage result(halves.size());
        for(unsigned i=0;i<halves.size();++i)for(unsigned channel=0;channel<4;++channel)
        {result[i][channel]=halves[i][channel];REQUIRE(std::isfinite(result[i][channel]));}
        return result;
    }
};

struct TileMetrics {double mean=0,mae=0,centered_rms=0,gradient_rms=0,luminance_bias=0;};
TileMetrics tileMetrics(const TileImage& actual,const TileImage& reference)
{
    REQUIRE(actual.size()==TW*TH);REQUIRE(reference.size()==actual.size());
    constexpr unsigned margin=12;constexpr double samples=(TW-2*margin)*(TH-2*margin)*3;
    TileMetrics result;double sum=0,square=0,gradient=0;unsigned gradients=0;
    constexpr double luminance[3]{.2126,.7152,.0722};
    for(unsigned y=margin;y<TH-margin;++y)for(unsigned x=margin;x<TW-margin;++x)for(unsigned c=0;c<3;++c)
    {
        const auto i=y*TW+x;const double d=actual[i][c]-reference[i][c];
        result.mean+=actual[i][c]/samples;result.mae+=std::abs(d)/samples;sum+=d;square+=d*d;
        result.luminance_bias+=d*luminance[c]/(samples/3.);
        for(unsigned offset:{1u,TW})
        {const double e=(actual[i+offset][c]-reference[i+offset][c])-d;gradient+=e*e;++gradients;}
    }
    result.centered_rms=std::sqrt(std::max(0.,square/samples-(sum/samples)*(sum/samples)));
    result.gradient_rms=std::sqrt(gradient/gradients);return result;
}
void tileSave(const std::filesystem::path& path,const TileImage& values)
{
    std::ofstream file(path,std::ios::binary);REQUIRE(file.good());
    file.write(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(TilePixel));REQUIRE(file.good());
}
}

TEST_CASE("Constant PBR receivers characterize rough SSR before reconstruction and presentation",
          "[metal][tile-sampling][tile-rough-ssr]")
{
    TileSsrFixture f;
    std::filesystem::path directory;
    if(const char* p=std::getenv("ALCHEMY_TILE_SAMPLING_OUTPUT"))
    {directory=p;std::filesystem::create_directories(directory);}
    std::ofstream csv;
    if(!directory.empty())
    {
        csv.open(directory/"tile-rough-ssr.csv");REQUIRE(csv.good());csv<<std::setprecision(17);
        csv<<"case,roughness,angle_radians,world,phase,jitter_x,jitter_y,low_mean,reference_mean,low_vs_reference_mae,centered_residual_rms,residual_gradient_rms,low_ssr_contribution,reference_ssr_contribution,matched_phase_mae\n";
        std::ofstream metadata(directory/"metadata.json");REQUIRE(metadata.good());
        metadata<<"{\"scope\":\"authored raw HDR SSR characterization; no owner-scene attribution\","
                <<"\"input_width\":"<<TW<<",\"input_height\":"<<TH
                <<",\"jitter_policy_output_width\":"<<TW*3/2<<",\"jitter_policy_output_height\":"<<TH*3/2
                <<",\"phases\":"<<TP<<",\"worlds_per_case\":"<<TP*2
                <<",\"budgets\":[0,4,128],\"roughness\":[0.15,0.35],\"plane_tilt_degrees\":[60,65],"
                <<"\"material_mip_bias\":"<<std::setprecision(17)<<std::log2(2.f/3.f)-1.f
                <<",\"constant_material_maps\":true,\"reconstruction_encoded\":false,"
                <<"\"display_presentation_encoded\":false,\"reference_is_same_shader_high_budget\":true,"
                <<"\"storage\":\"lower-row-first RGBA32Float dumps\",\"rgb_roi_margin\":12}\n";
        REQUIRE(metadata.good());
    }
    unsigned case_index=0;
    for(float roughness:{.15f,.35f})for(float angle:{1.0471975512f,1.1344640138f})
    {
        CAPTURE(roughness,angle);auto draw=f.receiverBatch(roughness,angle);
        ALMetalTemporalWorldHistory phases;std::array<TileImage,TP> first_cycle;
        double maximum_contribution=0,mean_error=0,mean_grain=0,maximum_luminance_bias=0;
        for(unsigned world=0;world<TP*2;++world)
        {
            ALMetalTemporalWorldHistory::FrameInput input;input.world_id=world+1;
            input.input_width=TW;input.input_height=TH;input.output_width=TW*3/2;input.output_height=TH*3/2;
            input.mode=8;input.reverse_z=true;input.jitter_phase_count=TP;
            std::copy(f.projection.begin(),f.projection.end(),input.camera_mvp);
            REQUIRE(phases.beginFrame(input,&f.error));
            ALMetalWorldGBufferScene opaque;opaque.width=TW;opaque.height=TH;
            ALMetalWorldOpaquePostDeferredScene forward;forward.width=TW;forward.height=TH;
            REQUIRE(phases.stage(opaque,forward,&f.error));const auto jitter=phases.jitterPixels();
            auto raster=f.projection;alMetalApplyTemporalJitter(raster.data(),jitter[0],jitter[1],TW,TH);
            f.data->beginFrame(++f.data_world);auto receiver=f.receiver(draw,raster);
            if(world==0)f.checkReceiver(*receiver,roughness);
            const auto off=f.consume(*receiver,raster,0),low=f.consume(*receiver,raster,4),reference=f.consume(*receiver,raster,128);
            const auto metric=tileMetrics(low,reference),control=tileMetrics(low,off),reference_control=tileMetrics(reference,off);
            maximum_contribution=std::max(maximum_contribution,control.mae);
            mean_error+=metric.mae/(TP*2);mean_grain+=metric.gradient_rms/(TP*2);
            maximum_luminance_bias=std::max(maximum_luminance_bias,std::abs(metric.luminance_bias));
            double matched=0;
            if(world<TP)first_cycle[world]=low;
            else {matched=tileMetrics(low,first_cycle[world%TP]).mae;CHECK(matched==0.);}
            REQUIRE(phases.commit(world+1,&f.error));
            if(csv.is_open())
                csv<<case_index<<','<<roughness<<','<<angle<<','<<world<<','<<world%TP<<','<<jitter[0]<<','<<jitter[1]<<','
                   <<metric.mean<<','<<tileMetrics(reference,off).mean<<','<<metric.mae<<','<<metric.centered_rms<<','<<metric.gradient_rms<<','
                   <<control.mae<<','<<reference_control.mae<<','<<matched<<'\n';
            if(!directory.empty())
            {
                const auto prefix="case-"+std::to_string(case_index)+"-world-"+std::to_string(world);
                tileSave(directory/(prefix+"-ssr-off.rgba32f"),off);
                tileSave(directory/(prefix+"-budget4.rgba32f"),low);
                tileSave(directory/(prefix+"-budget128-reference.rgba32f"),reference);
            }
        }
        INFO("Raw HDR tile characterization roughness="<<roughness<<" angle="<<angle
             <<" maxSSRcontribution="<<maximum_contribution<<" budget4/reference MAE="<<mean_error
             <<" residual gradient RMS="<<mean_grain);
        std::cout<<std::setprecision(10)<<"Raw HDR tile SSR: roughness="<<roughness<<" angle="<<angle
                 <<" maximum contribution="<<maximum_contribution<<" budget4/reference MAE="<<mean_error
                 <<" residual gradient RMS="<<mean_grain<<'\n';
        // This guards the experiment, not a quality pass: no SSR hit means
        // nothing was characterized. Do not turn the measured residual into
        // an owner-scene attribution or a passing reconstruction threshold.
        REQUIRE(maximum_contribution>1.e-5);
        // Frozen authored-signal quality limits: baseline budget4's one-ray
        // .15 cases measured gradient RMS .05672/.08372, versus .02386/.03797
        // with four rays. The absolute .045 limit retains headroom and makes
        // both original counterexamples fail. The .01 MAE limit additionally
        // rejects its .01678 worst-angle error. These are fixture limits, not
        // claims of Native parity or acceptance of the owner's actual tiles.
        if(roughness<.2f)
        {
            CHECK(mean_grain<.045);
            CHECK(mean_error<.01);
        }
        // A uniform fade or removal of SSR cannot qualify by reducing edge
        // energy: preserve mean luminance against the same high-budget arm
        // for every phase, within .001 linear HDR (~.6% of this signal).
        CHECK(maximum_luminance_bias<.001);
        ++case_index;
    }
}
