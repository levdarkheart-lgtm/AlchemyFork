/** Production forward color visibility must agree with Temporal depth, motion and rejection. */
#include "almetal_world_inputs_fixture.h"
#include "almetaltemporalworldhistory.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
using Color = std::array<float, 4>;
using Matrix = std::array<float, 16>;
Matrix multiply(const Matrix& a, const Matrix& b)
{
    Matrix out{};
    for (unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) for(unsigned k=0;k<4;++k)
        out[c*4+r] += a[k*4+r]*b[c*4+k];
    return out;
}
ALMetalWorldTemporalPreviousDraw previous(const ALMetalWorldSimpleBatch& b)
{
    ALMetalWorldTemporalPreviousDraw p;
    std::copy_n(b.modelview_projection,16,p.modelview_projection);
    std::copy_n(b.texture_matrix,16,p.texture_matrix);
    p.valid=true; return p;
}
std::vector<std::array<float,2>> productionJitters(unsigned count)
{
    ALMetalTemporalWorldHistory history;
    ALMetalTemporalWorldHistory::FrameInput input;
    input.input_width=input.input_height=side;input.output_width=input.output_height=side*3/2;
    input.mode=6;identity(input.camera_mvp);
    ALMetalWorldGBufferScene empty;
    std::vector<std::array<float,2>> out;
    for(unsigned phase=0;phase<count;++phase)
    {
        input.world_id=phase+1;REQUIRE(history.beginFrame(input));out.push_back(history.jitterPixels());
        REQUIRE(history.stage(empty,nullptr));REQUIRE(history.commit(input.world_id));
    }
    return out;
}
struct VisibilitySettings
{
    bool perspective=false,reverse_z=false,dynamic_sky=false,water_plane=false;
    float camera_translation=0.f,previous_camera_translation=0.f,forward_translation=0.f;
    float alpha_water_plane_offset=std::numeric_limits<float>::quiet_NaN();
    ALMetalTemporalScaler* temporal=nullptr;
    bool temporal_reset=false;
    int invalid_forward=-1;
    std::array<float,2> jitter{};
};
struct VisibilityPixels
{
    std::vector<Color> before,color,reconstructed;
    std::vector<std::array<__fp16,2>> motion;
    std::vector<std::uint8_t> reactive;
    std::vector<float> depth,real_depth;
};
struct VisibilityFixture : Fixture
{
    ALMetalWorldAlphaEnvironment environment;
    ALMetalWorldLegacyAlphaLighting lighting;
    const ALMetalTexture* black_environment=nullptr;
    std::uint64_t frame=10;
    VisibilityFixture()
    {
        environment.water_plane[2]=1.f;environment.water_plane[3]=-100.f;
        environment.water_fog[0]=environment.water_fog[2]=1.f;
        environment.glow_max_y[0]=1.f;environment.glow_max_y[2]=-1.f;environment.glow_max_y[3]=1.f;
        environment.light_direction[2]=1.f;
        environment.atmospheric[1]=environment.atmospheric[2]=environment.atmospheric[3]=1.f;
        environment.atmos_sunlit[3]=environment.atmos_sunlit_linear[3]=1.f;
        environment.environment[0]=1.f;
        for(unsigned i=0;i<3;++i)environment.eye_to_sky[i][i]=1.f;
        lighting.sun_direction[2]=1.f;lighting.parameters[0]=128.f;
        lighting.parameters[1]=.004f;lighting.parameters[2]=1.f;
        ALMetalTextureDescriptor cube;cube.type=ALMetalTextureType::texture_cube;
        cube.format=ALMetalPixelFormat::rgba8_unorm;cube.usage=ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> black=ALMetalTexture::create(*device,cube,&error);REQUIRE(black);
        auto command=device->makeCommandBuffer("Visibility fixture neutral environment",&error);REQUIRE(command);
        const std::uint8_t pixel[4]{0,0,0,255};
        for(unsigned face=0;face<6;++face)REQUIRE(black->upload(*command,0,face,{},pixel,4,&error));
        REQUIRE(command->commitAndWait(&error));black_environment=black.get();resources->textures.push_back(black);
    }
    const ALMetalTexture* flatColor(Color color)
    {
        std::array<__fp16,4> pixel;std::copy(color.begin(),color.end(),pixel.begin());
        return texture(ALMetalPixelFormat::rgba16_float,1,pixel.data(),8);
    }
    ALMetalWorldSimpleBatch card(unsigned family,float z,float extent,float alpha,
        bool backface=false,bool double_sided=false,Color rgb={.15f,.8f,.1f,1.f},std::uint8_t vertex_alpha=255)
    {
        Geometry geometry{};
        const float xy[4][2]{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}};
        for(unsigned i=0;i<4;++i)
        {
            std::copy_n(xy[i],2,geometry.positions[i]);geometry.positions[i][2]=z;geometry.positions[i][3]=0.f;
            geometry.normals[i][2]=1.f;geometry.uv[i][0]=(i==1||i==2);geometry.uv[i][1]=i>=2;
            std::fill_n(geometry.colors[i],4,std::uint8_t(255));geometry.colors[i][3]=vertex_alpha;
        }
        const std::array<std::uint16_t,6> front{0,1,2,0,2,3},back{0,2,1,0,3,2};
        auto b=batch(family);b.vertices=buffer(&geometry,sizeof(geometry));b.indices=buffer(backface?back.data():front.data(),sizeof(front));
        b.vertex_count=4;b.index_count=6;b.post_water_alpha=true;
        b.pre_water_fullbright=family==0;b.pre_water_pbr=family==2;b.pre_water_minimum_alpha=.004f;
        b.double_sided=double_sided;rgb[3]=alpha;b.diffuse_textures[0]=flatColor(rgb);
        if(family==2){b.pbrSlots()[0].alpha_cutoff=-1.f;b.pbrSlots()[0].metallic_factor=0.f;}
        return b;
    }
    ALMetalWorldSimpleBatch wall(bool perspective=false)
    {
        auto b=card(0,perspective?-4.f:.5f,perspective?3.8f:.95f,1.f,false,false,{.2f,.15f,.55f,1.f});
        b.post_water_alpha=false;b.pre_water_fullbright=false;return b;
    }
    static Matrix projection(bool reverse)
    {
        Matrix p{};p[0]=p[5]=1.f;p[11]=-1.f;
        p[10]=reverse?0.f:-101.f/99.f;p[14]=reverse?1.f:-200.f/99.f;return p;
    }
    static Matrix inverseProjection(bool reverse)
    {
        Matrix p{};p[0]=p[5]=1.f;p[11]=reverse?1.f:-99.f/200.f;
        p[14]=-1.f;p[15]=reverse?0.f:101.f/200.f;return p;
    }
    static float deviceDepth(float z,bool reverse)
    { return reverse?1.f/-z:(((-101.f/99.f)*z-200.f/99.f)/-z)*.5f+.5f; }
    std::vector<Color> colors(const ALMetalTexture& source)
    {
        const auto width=source.width(),height=source.height();
        std::vector<std::array<__fp16,4>> raw(width*height);
        REQUIRE(source.readback(*device,0,0,{},raw.data(),width*8,&error));
        std::vector<Color> out(raw.size());
        for(std::size_t i=0;i<out.size();++i)std::copy(raw[i].begin(),raw[i].end(),out[i].begin());
        return out;
    }
    ALMetalWorldModernSkyScene sky(const Matrix& inverse,bool reverse)
    {
        ALMetalWorldModernSkyScene scene;scene.width=scene.height=side;
        auto& c=scene.constants;std::copy(inverse.begin(),inverse.end(),c.inverse_projection);
        for(unsigned i=0;i<3;++i)c.eye_to_sky[i][i]=c.hdri_rotation[i][i]=1.f;
        c.config[0]=1;c.config[1]=1;c.config[2]=reverse;
        c.dome.camera_local[3]=15000.f;c.dome.light_density[1]=1.f;c.dome.light_density[3]=.001f;
        c.dome.glow[0]=1.f;c.dome.glow[2]=-1.f;c.dome.glow[3]=1000.f;
        c.dome.blue_horizon[0]=.4f;c.dome.blue_horizon[1]=.3f;c.dome.blue_horizon[2]=.2f;c.dome.blue_horizon[3]=.1f;
        for(unsigned i=0;i<3;++i)
        {
            c.dome.haze_extinction[i]=.5f;c.dome.blue_weight[i]=c.dome.haze_weight[i]=1.f;
            c.dome.sky_light[i]=.5f;c.dome.sky_ambient[i]=.2f;
            c.dome.sky_light_atten[i]=c.dome.sky_ambient_below[i]=.1f;
        }
        c.dome.sky_ambient_below[3]=.2f;c.dome.optics[1]=10.f;
        scene.stars_texture=flatColor({.2f,.35f,.6f,1.f});return scene;
    }
    VisibilityPixels render(std::vector<ALMetalWorldSimpleBatch> opaque,
        std::vector<ALMetalWorldSimpleBatch> forward,VisibilitySettings settings={})
    {
        data->beginFrame(++frame);
        Matrix p{},inverse{};identity(p.data());identity(inverse.data());
        if(settings.perspective){p=projection(settings.reverse_z);inverse=inverseProjection(settings.reverse_z);}
        std::vector<ALMetalWorldTemporalPreviousDraw> old_opaque,old_forward;
        const auto prepare=[&](auto& draws,auto& old,bool is_forward)
        {
            for(auto& b:draws)
            {
                if(settings.perspective)std::copy(p.begin(),p.end(),b.modelview_projection);
                old.push_back(previous(b));
                if(settings.perspective)
                {
                    old.back().modelview_projection[12]-=settings.previous_camera_translation;
                    b.modelview_projection[12]-=settings.camera_translation;b.modelview[12]-=settings.camera_translation;
                }
                if(is_forward)b.modelview_projection[12]+=settings.forward_translation;
            }
        };
        prepare(opaque,old_opaque,false);prepare(forward,old_forward,true);
        if(settings.invalid_forward>=0)old_forward.at(settings.invalid_forward).valid=false;
        auto world=scene();world.batches=opaque.data();world.batch_count=opaque.size();world.hdr=true;
        world.reverse_z=settings.reverse_z;world.material_mip_bias=std::log2(2.f/3.f)-1.f;
        std::copy(settings.jitter.begin(),settings.jitter.end(),world.temporal_jitter_pixels);
        auto gbuffer=renderer->render(world,&error);INFO(error);REQUIRE(gbuffer);
        auto snapshot=renderer->prepareTemporalOpaqueDepth(side,side,&error);REQUIRE(snapshot);
        auto composed=renderer->prepareTemporalTarget(side,side,false,&error);REQUIRE(composed);
        REQUIRE(renderer->beginCommandBatch("Visibility fixture main preparation",&error));
        REQUIRE(renderer->captureTemporalOpaqueDepth(*gbuffer->depthStencilTexture(),*snapshot,&error));
        REQUIRE(renderer->resolveTemporalColor(*gbuffer,*gbuffer,*composed,&error));
        auto modern_sky=sky(inverse,settings.reverse_z);
        if(settings.dynamic_sky)REQUIRE(renderer->compositeModernSky(*composed,*gbuffer->depthStencilTexture(),modern_sky,&error));
        REQUIRE(renderer->commitCommandBatch(&error));
        VisibilityPixels out;out.before=colors(*composed->colorTexture(0));
        ALMetalWorldOpaquePostDeferredScene forward_scene;
        forward_scene.width=forward_scene.height=side;forward_scene.reverse_z=settings.reverse_z;forward_scene.hdr=true;
        forward_scene.material_mip_bias=world.material_mip_bias;forward_scene.batches=forward.data();forward_scene.batch_count=forward.size();
        auto frame_environment=environment;
        if(!forward.empty() && forward.front().pre_water_alpha)
            frame_environment.water_plane[3]=100.f; // Real pre-water color keeps the opposite halfspace.
        if(std::isfinite(settings.alpha_water_plane_offset))frame_environment.water_plane[3]=settings.alpha_water_plane_offset;
        forward_scene.alpha_environment=&frame_environment;forward_scene.alpha_lighting=&lighting;forward_scene.legacy_environment=black_environment;
        auto colored=forward;for(auto& b:colored)alMetalApplyTemporalJitter(b.modelview_projection,settings.jitter[0],settings.jitter[1],side,side);
        auto descriptor=composed->descriptor();descriptor.colors[0].texture=composed->colorTexture(0);
        descriptor.depth_stencil_format=gbuffer->descriptor().depth_stencil_format;
        descriptor.depth_stencil_shader_read=true;descriptor.depth_stencil_texture=gbuffer->depthStencilTexture();
        descriptor.label="Visibility fixture production borrowed-depth color continuation";
        auto continuation=ALMetalRenderTarget::create(*device,descriptor,&error);INFO(error);REQUIRE(continuation);
        auto color_scene=forward_scene;color_scene.batches=colored.data();color_scene.gbuffer=gbuffer.get();color_scene.composed_lighting=continuation.get();
        auto color=renderer->renderOpaquePostDeferred(color_scene,std::move(continuation),&error);INFO(error);REQUIRE(color);
        out.color=colors(*color->colorTexture(0));
        ALMetalWorldTemporalMotionScene motion;
        motion.width=motion.height=side;motion.opaque_scene=&world;motion.forward_scene=&forward_scene;
        motion.depth=gbuffer->depthStencilTexture();motion.opaque_depth=snapshot->colorTexture(0);
        motion.previous_opaque=old_opaque.data();motion.previous_opaque_count=old_opaque.size();
        motion.previous_forward=old_forward.data();motion.previous_forward_count=old_forward.size();
        motion.reverse_z=settings.reverse_z;motion.reset=settings.temporal_reset;
        std::copy(settings.jitter.begin(),settings.jitter.end(),motion.jitter_pixels);
        Matrix translation{};identity(translation.data());translation[12]=settings.camera_translation-settings.previous_camera_translation;
        const auto reproject=multiply(multiply(p,translation),inverse);
        std::copy(reproject.begin(),reproject.end(),motion.clip_to_previous_clip);
        std::copy(inverse.begin(),inverse.end(),motion.inverse_projection);
        if(settings.dynamic_sky)motion.modern_sky=&modern_sky;
        // Exercise the existing water-rejection plane contract. The blue
        // backdrop stands for visible water; the water-surface shader is not
        // under test here. The plane at eyeZ=-3 is behind the leaf atZ=-2.
        motion.reject_water=settings.water_plane;motion.eye_water_plane[2]=1.f;motion.eye_water_plane[3]=3.f;
        auto inputs=renderer->renderTemporalMotion(motion,&error);INFO(error);REQUIRE(inputs);
        if(settings.temporal)
        {
            ALMetalTemporalScalerDescriptor descriptor{side,side,side*3/2,side*3/2};
            auto raw=renderer->prepareTemporalTarget(descriptor.output_width,descriptor.output_height,true,&error);
            auto resolved=renderer->prepareTemporalTarget(descriptor.output_width,descriptor.output_height,false,&error);
            REQUIRE(raw);REQUIRE(resolved);
            ALMetalTemporalScalerFrame frame;frame.depth_reversed=settings.reverse_z;frame.reset=settings.temporal_reset;
            frame.jitter_x=settings.jitter[0];frame.jitter_y=settings.jitter[1];ALMetalTemporalScaler::Ticket ticket=0;
            REQUIRE(renderer->beginCommandBatch("Perspective alpha actual Temporal output",&error));
            REQUIRE(renderer->encodeTemporalUpscale(*settings.temporal,descriptor,*color->colorTexture(0),*inputs,*raw,frame,ticket,&error));
            INFO(error);REQUIRE(renderer->resolveTemporalColor(*color,*raw,*resolved,&error,settings.jitter.data()));
            REQUIRE(renderer->commitCommandBatch(&error));REQUIRE(settings.temporal->submitted(ticket,&error));
            out.reconstructed=colors(*resolved->colorTexture(0));
        }
        out.motion.resize(side*side);out.reactive.resize(side*side);out.depth.resize(side*side);out.real_depth.resize(side*side);
        REQUIRE(inputs->colorTexture(0)->readback(*device,0,0,{},out.motion.data(),side*4,&error));
        REQUIRE(inputs->colorTexture(1)->readback(*device,0,0,{},out.reactive.data(),side,&error));
        REQUIRE(inputs->colorTexture(2)->readback(*device,0,0,{},out.depth.data(),side*4,&error));
        REQUIRE(gbuffer->depthStencilTexture()->readback(*device,0,0,{},out.real_depth.data(),side*4,&error));
        return out;
    }
};
float colorDistance(const Color& a,const Color& b)
{float d=0;for(unsigned c=0;c<3;++c)d+=std::abs(a[c]-b[c]);return d;}
constexpr unsigned Center=16*side+16;
}

TEST_CASE("Temporal forward visibility matches generic and PBR color culling", "[almetal][temporal-visibility][forward-visibility-cull]")
{
    VisibilityFixture f;
    for(unsigned family:{0u,2u})for(bool double_sided:{false,true})for(bool backface:{false,true})
    {
        if(family==0&&double_sided)continue; // Generic BLEND has no double-sided production option.
        for(bool pre_water:{false,true})for(auto jitter:productionJitters(3))
        {
            CAPTURE(family,double_sided,backface,pre_water,jitter[0],jitter[1]);
            auto leaf=f.card(family,-.2f,.7f,1.f,backface,double_sided);
            if(pre_water){leaf.pre_water_alpha=true;leaf.post_water_alpha=false;}
            VisibilitySettings settings;settings.jitter=jitter;settings.forward_translation=.125f;
            const auto actual=f.render({f.wall()},{leaf},settings);
            const bool visible=!backface||double_sided;
            if(!visible)
            {
                CHECK(actual.color==actual.before); // Demonstrate real color culling, not a face-name assumption.
                const auto control=f.render({f.wall()},{},settings);
                CHECK(actual.motion==control.motion);CHECK(actual.depth==control.depth);CHECK(actual.reactive==control.reactive);
                CHECK(actual.real_depth==control.real_depth);
            }
            else
            {
                CHECK(colorDistance(actual.color[Center],actual.before[Center])>.01f);
                CHECK(std::abs(actual.depth[Center]-.4f)<1.e-6f);
                CHECK(std::abs(float(actual.motion[Center][0])+2.f)<.025f);
                CHECK(std::abs(actual.real_depth[Center]-(pre_water?.4f:.75f))<1.e-6f);
            }
        }
    }
}

TEST_CASE("Temporal perspective alpha distinguishes hidden wall motion from contributing parallax", "[almetal][temporal-visibility][forward-visibility-parallax]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(unsigned family:{0u,2u})for(auto jitter:productionJitters(3))
    {
        CAPTURE(reverse,family,jitter[0],jitter[1]);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;
        settings.camera_translation=.1f;settings.jitter=jitter;
        std::array<VisibilityPixels,3> pixels;
        const float alpha[3]{0.f,.5f,1.f};
        for(unsigned arm=0;arm<3;++arm)pixels[arm]=f.render({f.wall(true)},{f.card(family,-2.f,1.6f,alpha[arm])},settings);
        CHECK(colorDistance(pixels[2].color[Center],pixels[0].color[Center])>.01f);
        for(unsigned c=0;c<3;++c)CHECK(std::abs(pixels[1].color[Center][c]-(pixels[0].color[Center][c]+pixels[2].color[Center][c])*.5f)<.006f);
        CHECK(std::abs(float(pixels[0].motion[Center][0])-.4f)<.025f);
        CHECK(std::abs(float(pixels[2].motion[Center][0])-.8f)<.025f);
        CHECK(std::abs(pixels[2].depth[Center]-VisibilityFixture::deviceDepth(-2.f,reverse))<1.e-6f);
        CHECK(pixels[0].reactive[Center]==0);CHECK(pixels[1].reactive[Center]>0);CHECK(pixels[2].reactive[Center]==0);
    }
}

TEST_CASE("Temporal hidden dynamic backgrounds reject only their visible alpha contribution", "[almetal][temporal-visibility][forward-visibility-background]")
{
    VisibilityFixture f;
    for(bool water:{false,true})for(bool reverse:{false,true})for(unsigned family:{0u,2u})
    {
        CAPTURE(water,reverse,family);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;
        settings.dynamic_sky=!water;settings.water_plane=water;settings.jitter=productionJitters(2).back();
        for(float alpha:{0.f,.5f,1.f})
        {
            CAPTURE(alpha);
            const auto pixels=f.render(water?std::vector<ALMetalWorldSimpleBatch>{f.wall(true)}:std::vector<ALMetalWorldSimpleBatch>{},
                {f.card(family,-2.f,1.6f,alpha)},settings);
            if(alpha==0.f)CHECK(pixels.color==pixels.before);else CHECK(colorDistance(pixels.color[Center],pixels.before[Center])>.01f);
            CHECK(pixels.reactive[Center]==(alpha==1.f?0:255));
            CHECK(pixels.reactive[0]==255); // Uncovered sky/water remains conservatively reactive.
            if(alpha==1.f)CHECK(std::abs(pixels.depth[Center]-VisibilityFixture::deviceDepth(-2.f,reverse))<1.e-6f);
        }
    }
}

TEST_CASE("Temporal overlapping alpha follows the submitted color composite and preserves visible unsafe layers", "[almetal][temporal-visibility][forward-visibility-overlap]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(bool far_last:{false,true})for(bool pre_water:{false,true})
    {
        CAPTURE(reverse,far_last,pre_water);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;settings.camera_translation=.1f;
        auto near=f.card(0,-2.f,1.6f,1.f,false,false,{.1f,.8f,.1f,1.f});
        auto far=f.card(0,-3.f,2.4f,1.f,false,false,{.8f,.1f,.1f,1.f});
        if(pre_water)for(auto* layer:{&near,&far}){layer->pre_water_alpha=true;layer->post_water_alpha=false;}
        std::vector<ALMetalWorldSimpleBatch> layers=far_last?std::vector{near,far}:std::vector{far,near};
        auto pixels=f.render({f.wall(true)},layers,settings);
        // Pre-water color writes depth, so a farther later layer fails the
        // hardware depth test. Post-water color is read-only and really does
        // let a later full-alpha layer replace the nearer earlier color.
        const bool far_visible=far_last&&!pre_water;
        const auto only=f.render({f.wall(true)},{far_visible?far:near},settings);
        CHECK(colorDistance(pixels.color[Center],only.color[Center])<.001f);
        const float z=far_visible?-3.f:-2.f;
        CHECK(std::abs(pixels.depth[Center]-VisibilityFixture::deviceDepth(z,reverse))<1.e-6f);
        CHECK(std::abs(float(pixels.motion[Center][0])-(.1f/-z)*(.5f*side))<.025f);
        CHECK(pixels.reactive[Center]==0);
        // A hidden invalid far layer cannot poison an opaque nearer color.
        if(!far_last)
        {
            settings.invalid_forward=0;auto hidden=f.render({f.wall(true)},layers,settings);
            CHECK(hidden.reactive[Center]==0);
            settings.invalid_forward=1;auto visible=f.render({f.wall(true)},layers,settings);
            CHECK(visible.reactive[Center]==255);
        }
    }
}

TEST_CASE("Temporal partial overlap preserves contributing parallax and full coverage hides it", "[almetal][temporal-visibility][forward-visibility-overlap]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(bool full_near:{false,true})for(unsigned family:{0u,2u})
    {
        CAPTURE(reverse,full_near,family);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;settings.camera_translation=.1f;
        settings.jitter=productionJitters(3).back();
        auto far=f.card(0,-3.f,2.4f,full_near?.5f:1.f,false,false,{.8f,.1f,.1f,1.f});
        auto near=f.card(family,-2.f,1.6f,full_near?1.f:.5f);
        const auto pixels=f.render({f.wall(true)},{far,near},settings);
        if(full_near)
        {
            const auto control=f.render({f.wall(true)},{near},settings);
            CHECK(colorDistance(pixels.color[Center],control.color[Center])<.001f);
            CHECK(pixels.reactive[Center]==0);
        }
        else
        {
            const auto far_only=f.render({f.wall(true)},{far},settings);
            const auto near_full=f.render({f.wall(true)},{f.card(family,-2.f,1.6f,1.f)},settings);
            for(unsigned c=0;c<3;++c)CHECK(std::abs(pixels.color[Center][c]-(far_only.color[Center][c]+near_full.color[Center][c])*.5f)<.006f);
            CHECK(pixels.reactive[Center]==255);
        }
        CHECK(std::abs(pixels.depth[Center]-VisibilityFixture::deviceDepth(-2.f,reverse))<1.e-6f);
    }
}

TEST_CASE("Temporal trailing depth-only companions preserve visible color history despite borrowed depth writes", "[almetal][temporal-visibility][forward-visibility-depth-only]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(float companion_alpha:{.1f,.5f})
    {
        CAPTURE(reverse,companion_alpha);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;settings.camera_translation=.1f;
        auto depth_only=f.card(0,-2.f,1.6f,companion_alpha);
        depth_only.post_water_depth_only=true;depth_only.pre_water_minimum_alpha=.33f;
        const auto far=f.card(0,-3.f,2.4f,1.f);
        // Production orders all color draws before post-water depth-only
        // companions. A companion may update the borrowed G-buffer depth,
        // but contributes no color and must not replace that color's history.
        const auto pixels=f.render({f.wall(true)},{far,depth_only},settings);
        const auto control=f.render({f.wall(true)},{far},settings);
        CHECK(colorDistance(pixels.color[Center],control.color[Center])<.001f);
        CHECK(std::abs(float(pixels.motion[Center][0])-float(control.motion[Center][0]))<.001f);
        CHECK(std::abs(pixels.depth[Center]-control.depth[Center])<1.e-6f);
        CHECK(pixels.reactive[Center]==control.reactive[Center]);
        CHECK(std::abs(pixels.real_depth[Center]-VisibilityFixture::deviceDepth(companion_alpha>.33f?-2.f:-4.f,reverse))<1.e-6f);
    }
}

TEST_CASE("Temporal opaque forward blockers preserve their color depth visibility before alpha", "[almetal][temporal-visibility][forward-visibility-opaque-blocker]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(bool shiny:{false,true})for(float source_alpha:{0.f,1.f})
    {
        CAPTURE(reverse,shiny,source_alpha);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;settings.camera_translation=.1f;
        auto blocker=f.card(0,-2.f,1.6f,source_alpha,false,false,{.75f,.5f,.15f,1.f});
        blocker.post_water_alpha=false;blocker.pre_water_fullbright=false;
        blocker.impostor_fullbright=!shiny;blocker.impostor_shiny=shiny;
        const auto far=f.card(0,-3.f,2.4f,1.f);
        const auto alone=f.render({f.wall(true)},{blocker},settings);
        const auto pixels=f.render({f.wall(true)},{blocker,far},settings);
        CHECK(colorDistance(alone.color[Center],alone.before[Center])>.01f);
        CHECK(colorDistance(pixels.color[Center],alone.color[Center])<.001f);
        CHECK(std::abs(pixels.real_depth[Center]-VisibilityFixture::deviceDepth(-2.f,reverse))<1.e-6f);
        CHECK(std::abs(float(pixels.motion[Center][0])-float(alone.motion[Center][0]))<.001f);
        CHECK(std::abs(pixels.depth[Center]-alone.depth[Center])<1.e-6f);
        // These opaque families stay deliberately conservative. A color-hidden
        // later qualified alpha must not clear their rejection or claim its
        // own surface, including when opaque RGB carries zero alpha/shininess.
        CHECK(alone.reactive[Center]==255);CHECK(pixels.reactive[Center]==255);
    }
}

TEST_CASE("Temporal fullbright masks use texture coverage without vertex shininess", "[almetal][temporal-visibility][forward-visibility-opaque-mask]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(std::uint8_t vertex_alpha:{std::uint8_t(0),std::uint8_t(255)})
        for(float texture_alpha:{0.f,.49853515625f,1.f})
    {
        CAPTURE(reverse,unsigned(vertex_alpha),texture_alpha);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;settings.camera_translation=.1f;
        auto poster=f.card(0,-2.f,1.6f,texture_alpha,false,false,{.75f,.5f,.15f,1.f},vertex_alpha);
        poster.post_water_alpha=false;poster.pre_water_fullbright=false;poster.impostor_fullbright=true;
        poster.alpha_mask=true;poster.alpha_cutoff=.5f;
        // Fullbright-mask color always uses the original1/512 texture cutoff
        // compensation, irrespective of this generic G-buffer hint.
        poster.alpha_cutoff_bias=false;
        const auto far=f.card(0,-3.f,2.4f,1.f);
        const auto alone=f.render({f.wall(true)},{poster},settings);
        const auto pixels=f.render({f.wall(true)},{poster,far},settings);
        const bool covered=texture_alpha>=.5f-1.f/512.f;
        if(covered)
        {
            CHECK(colorDistance(alone.color[Center],alone.before[Center])>.01f);
            CHECK(colorDistance(pixels.color[Center],alone.color[Center])<.001f);
            CHECK(std::abs(pixels.real_depth[Center]-VisibilityFixture::deviceDepth(-2.f,reverse))<1.e-6f);
            CHECK(std::abs(float(pixels.motion[Center][0])-float(alone.motion[Center][0]))<.001f);
            CHECK(std::abs(pixels.depth[Center]-alone.depth[Center])<1.e-6f);
            CHECK(alone.reactive[Center]==255);CHECK(pixels.reactive[Center]==255);
        }
        else
        {
            CHECK(colorDistance(alone.color[Center],alone.before[Center])<.001f);
            const auto control=f.render({f.wall(true)},{far},settings);
            CHECK(colorDistance(pixels.color[Center],control.color[Center])<.001f);
            CHECK(std::abs(pixels.real_depth[Center]-VisibilityFixture::deviceDepth(-4.f,reverse))<1.e-6f);
            CHECK(std::abs(float(pixels.motion[Center][0])-float(control.motion[Center][0]))<.001f);
            CHECK(std::abs(pixels.depth[Center]-control.depth[Center])<1.e-6f);
            CHECK(pixels.reactive[Center]==0);
        }
    }
}

namespace
{
void dumpPerspective(const std::filesystem::path& directory,const std::string& name,const std::vector<Color>& image,unsigned width)
{
    if(directory.empty())return;
    std::ofstream raw(directory/(name+".rgba32f"),std::ios::binary);REQUIRE(raw.good());
    raw.write(reinterpret_cast<const char*>(image.data()),image.size()*sizeof(Color));REQUIRE(raw.good());
    std::ofstream ppm(directory/(name+".ppm"),std::ios::binary);REQUIRE(ppm.good());
    ppm<<"P6\n"<<width<<' '<<image.size()/width<<"\n255\n";
    for(const auto& pixel:image)for(unsigned c=0;c<3;++c)
    {
        const float linear=std::clamp(pixel[c],0.f,1.f);
        const float srgb=linear<=.0031308f?linear*12.92f:1.055f*std::pow(linear,1.f/2.4f)-.055f;
        const auto byte=std::uint8_t(std::lround(srgb*255.f));ppm.write(reinterpret_cast<const char*>(&byte),1);
    }
    REQUIRE(ppm.good());
}
}
TEST_CASE("Temporal perspective full and partial alpha settle after camera motion without persistent old trails", "[almetal][temporal-visibility][forward-visibility-output]")
{
    constexpr unsigned Phases=18,Warm=36,Moving=18,Stopped=36,Total=Warm+Moving+Stopped;
    constexpr float EndCamera=.6f,LeafEyeZ=-2.f,LeafExtent=1.f;
    const auto jitters=productionJitters(Phases);
    std::filesystem::path directory;
    if(const char* path=std::getenv("ALCHEMY_TEMPORAL_VISIBILITY_OUTPUT"))
    {directory=path;std::filesystem::create_directories(directory);}
    std::ofstream metrics;
    if(!directory.empty())
    {metrics.open(directory/"perspective-stop.csv");REQUIRE(metrics.good());metrics<<"alpha,stopped_worlds,changed_pixels,phase_matched_mean_rgb_error,old_history_fraction,disoccluded_pixels,disoccluded_mean_rgb_error,disoccluded_old_history_fraction\n";}
    for(float alpha:{1.f,.5f})
    {
        CAPTURE(alpha);
        VisibilityFixture f;
        auto moving=ALMetalTemporalScaler::create(*f.device,&f.error);
        auto stationary=ALMetalTemporalScaler::create(*f.device,&f.error);REQUIRE(moving);REQUIRE(stationary);
        ALMetalTemporalScalerDescriptor descriptor{side,side,side*3/2,side*3/2};ALMetalTemporalScalerRequirements requirements;
        REQUIRE(moving->prepare(descriptor,requirements,&f.error));REQUIRE(stationary->prepare(descriptor,requirements,&f.error));
        std::array<std::vector<Color>,Phases> initial;
        const auto leaf=f.card(0,LeafEyeZ,LeafExtent,alpha),wall=f.wall(true);
        float previous_camera=0.f;
        for(unsigned frame=0;frame<Total;++frame)
        {
            const unsigned moved=frame<Warm?0u:std::min(frame-Warm+1u,Moving);
            const float camera=EndCamera*float(moved)/Moving;
            VisibilitySettings settings;settings.perspective=true;settings.reverse_z=true;
            settings.camera_translation=camera;settings.previous_camera_translation=previous_camera;
            settings.jitter=jitters[frame%Phases];settings.temporal=moving.get();settings.temporal_reset=frame==0;
            const auto current=f.render({wall},{leaf},settings);
            settings.camera_translation=settings.previous_camera_translation=EndCamera;settings.temporal=stationary.get();
            const auto reference=f.render({wall},{leaf},settings);
            const float expected=(camera-previous_camera)*(.5f*side)/2.f;
            if(frame>=Warm && frame<Warm+Moving)
            {
                CHECK(std::abs(float(current.motion[Center][0])-expected)<.002f);
                CHECK(unsigned(current.reactive[Center])==(alpha==1.f?0u:255u));
            }
            if(frame>=Phases && frame<Warm)initial[frame%Phases]=current.reconstructed;
            if(frame>=Warm+Moving)
            {
                CHECK(current.color==reference.color); // Exact same current raster inputs and jitter phase.
                CHECK(unsigned(current.reactive[Center])==0u);
            }
            const auto prefix=alpha==1.f?std::string("full"):std::string("partial");
            if(frame==Warm-1 || frame==Warm+Moving-1 || frame==Warm+Moving || frame==Warm+Moving+Phases-1 || frame==Total-1)
            {
                const auto label=prefix+"-world-"+std::to_string(frame);
                dumpPerspective(directory,label+"-input",current.color,side);
                dumpPerspective(directory,label+"-output",current.reconstructed,side*3/2);
                dumpPerspective(directory,label+"-stationary-reference",reference.reconstructed,side*3/2);
            }
            if(frame==Warm+Moving+Phases-1 || frame==Total-1)
            {
                const auto& old=initial[frame%Phases];REQUIRE(old.size()==current.reconstructed.size());
                double error=0,old_fraction=0,disoccluded_error=0,disoccluded_old_fraction=0;
                unsigned changed=0,disoccluded=0;
                constexpr unsigned OutputWidth=side*3/2;
                constexpr float half_output=OutputWidth*.5f;
                const float half_leaf=half_output*LeafExtent/-LeafEyeZ;
                const float old_left=half_output-half_leaf,old_right=half_output+half_leaf;
                const float final_left=old_left-EndCamera*half_output/-LeafEyeZ;
                const float final_right=old_right-EndCamera*half_output/-LeafEyeZ;
                for(std::size_t pixel=0;pixel<old.size();++pixel)
                {
                    const auto& ref=reference.reconstructed[pixel];
                    double numerator=0,denominator=0,pixel_error=0;
                    for(unsigned c=0;c<3;++c)
                    {
                        const double difference=current.reconstructed[pixel][c]-ref[c],direction=old[pixel][c]-ref[c];
                        pixel_error+=std::abs(difference);numerator+=difference*direction;denominator+=direction*direction;
                    }
                    const double fraction=denominator>1.e-12?std::clamp(numerator/denominator,0.,1.):0.;
                    const float x=float(pixel%OutputWidth)+.5f,y=float(pixel/OutputWidth)+.5f;
                    // Analytical old-only leaf interior, excluding one output
                    // pixel around both raster boundaries. This cannot dilute
                    // persistent trails with newly covered pixels or wall edges.
                    const bool old_inside=x>=old_left+1.f && x<=old_right-1.f &&
                        y>=old_left+1.f && y<=old_right-1.f;
                    const bool current_near=x>=final_left-1.f && x<=final_right+1.f;
                    if(old_inside && !current_near)
                    {disoccluded_error+=pixel_error;disoccluded_old_fraction+=fraction;++disoccluded;}
                    if(colorDistance(old[pixel],ref)<.045f)continue;
                    error+=pixel_error;old_fraction+=fraction;++changed;
                }
                REQUIRE(changed>10);REQUIRE(disoccluded==110);
                error/=double(changed*3);old_fraction/=changed;
                disoccluded_error/=double(disoccluded*3);disoccluded_old_fraction/=disoccluded;
                const unsigned stopped=frame-(Warm+Moving)+1;
                INFO("alpha="<<alpha<<", stopped="<<stopped<<", phase_error="<<error<<", old_history="<<old_fraction<<
                    ", disoccluded_error="<<disoccluded_error<<", disoccluded_old_history="<<disoccluded_old_fraction);
                if(metrics)metrics<<alpha<<','<<stopped<<','<<changed<<','<<error<<','<<old_fraction<<','<<
                    disoccluded<<','<<disoccluded_error<<','<<disoccluded_old_fraction<<'\n';
                CHECK(error<=1./255.);CHECK(old_fraction<=.02); // Retain the all-changed-region bounds too.
                CHECK(disoccluded_error<=1./255.);CHECK(disoccluded_old_fraction<=.02);
            }
            previous_camera=camera;
        }
        CHECK(moving->stats().submitted_calls==Total);CHECK(stationary->stats().submitted_calls==Total);
        CHECK(moving->stats().reset_calls==1);CHECK(stationary->stats().reset_calls==1);
    }
}

TEST_CASE("Temporal zero-alpha history preserves background inputs while retaining actual depth writes", "[almetal][temporal-visibility][forward-visibility-zero-alpha]")
{
    VisibilityFixture f;
    for(bool reverse:{false,true})for(unsigned family:{0u,2u})for(bool pre_water:{false,true})
    {
        CAPTURE(reverse,family,pre_water);
        VisibilitySettings settings;settings.perspective=true;settings.reverse_z=reverse;settings.camera_translation=.1f;
        auto zero=f.card(family,-2.f,1.6f,0.f);zero.pre_water_minimum_alpha=0.f;
        std::vector<ALMetalWorldSimpleBatch> layers{zero};
        if(pre_water)
        {
            layers.front().pre_water_alpha=true;layers.front().post_water_alpha=false;
            // Both draws pass their real opposite water halfspaces: nearZ=-2
            // on the pre-water side, farZ=-3 on the post-water side. Only the
            // zero-alpha pre-water depth write blocks the latter color.
            settings.alpha_water_plane_offset=2.5f;layers.push_back(f.card(0,-3.f,2.4f,1.f));
            const auto far_only=f.render({f.wall(true)},{layers.back()},settings);
            CHECK(colorDistance(far_only.color[Center],far_only.before[Center])>.01f);
        }
        const auto control=f.render({f.wall(true)},{},settings);
        const auto pixels=f.render({f.wall(true)},layers,settings);
        CHECK(colorDistance(pixels.color[Center],control.color[Center])<.001f);
        CHECK(std::abs(float(pixels.motion[Center][0])-float(control.motion[Center][0]))<.001f);
        CHECK(std::abs(pixels.depth[Center]-control.depth[Center])<1.e-6f);
        CHECK(unsigned(pixels.reactive[Center])==unsigned(control.reactive[Center]));
        CHECK(std::abs(pixels.real_depth[Center]-VisibilityFixture::deviceDepth(pre_water?-2.f:-4.f,reverse))<1.e-6f);
        if(!pre_water)
        {
            // Do not turn exact-zero handling into another alpha cutoff.
            auto tiny=f.card(family,-2.f,1.6f,1.f/1024.f);tiny.pre_water_minimum_alpha=0.f;
            const auto contributing=f.render({f.wall(true)},{tiny},settings);
            CHECK(colorDistance(contributing.color[Center],contributing.before[Center])>0.f);
            CHECK(unsigned(contributing.reactive[Center])==255);
        }
    }
}
