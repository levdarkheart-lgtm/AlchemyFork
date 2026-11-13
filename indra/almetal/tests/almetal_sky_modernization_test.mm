/**
 * @file almetal_sky_modernization_test.mm
 * @brief Final ray sky, depth metering and probe-less cube on the native GPU.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetal_sky_modernization_fixture.h"
namespace {
using namespace sky_test;
}
TEST_CASE("Native ray sky payload matches every shared CPU and MSL boundary", "[almetal][modern-sky][modern-sky-layout]")
{
    @autoreleasepool {
        Fixture f;
        auto library = ALMetalShaderLibrary::loadBuiltin(*f.device, &f.error); REQUIRE(library);
        ALMetalComputePipelineDescriptor descriptor;
        descriptor.library = library.get(); descriptor.function = "almetal_world_modern_sky_layout_probe";
        auto pipeline = ALMetalComputePipeline::create(*f.device, descriptor, &f.error); INFO(f.error); REQUIRE(pipeline);
        auto scene = f.scene(); auto& c = scene.constants;
        c.dome.camera_local[3] = 12345.5f; c.clouds.parameters[2] = .3125f;
        c.body[1] = .625f; c.transfer[3] = 1.125f; c.config[3] = 37;
        std::array<std::uint32_t, 19> actual{};
        auto output = f.buffer(actual);
        auto command = f.device->makeCommandBuffer("Verify ray sky constant ABI before drawing", &f.error); REQUIRE(command);
        auto encoder = ALMetalComputeEncoder::begin(*command, "Sky constant offsets and sentinels", &f.error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &f.error));
        REQUIRE(encoder->setBytes(0, &c, sizeof(c), &f.error));
        REQUIRE(encoder->setBuffer(1, *output, 0, &f.error));
        REQUIRE(encoder->dispatch(1,1,1,1,1,1,&f.error)); REQUIRE(encoder->end(&f.error));
        REQUIRE(command->commitAndWait(&f.error)); REQUIRE(output->read(0, actual.data(), sizeof(actual), &f.error));
        std::array<std::uint32_t, 19> expected{
            sizeof(c.dome), sizeof(c.clouds), sizeof(c),
            offsetof(ALMetalWorldModernSkyConstants,dome), offsetof(ALMetalWorldModernSkyConstants,clouds),
            offsetof(ALMetalWorldModernSkyConstants,sun_quad), offsetof(ALMetalWorldModernSkyConstants,moon_quad),
            offsetof(ALMetalWorldModernSkyConstants,bloom_quad), offsetof(ALMetalWorldModernSkyConstants,body),
            offsetof(ALMetalWorldModernSkyConstants,bloom_color), offsetof(ALMetalWorldModernSkyConstants,hdri_rotation),
            offsetof(ALMetalWorldModernSkyConstants,transfer), offsetof(ALMetalWorldModernSkyConstants,aurora),
            offsetof(ALMetalWorldModernSkyConstants,config)};
        std::memcpy(&expected[14], &c.dome.camera_local[3], 4);
        std::memcpy(&expected[15], &c.clouds.parameters[2], 4);
        std::memcpy(&expected[16], &c.body[1], 4);
        std::memcpy(&expected[17], &c.transfer[3], 4); expected[18] = c.config[3];
        CHECK(actual == expected);
    }
}
TEST_CASE("Native ray HDRI changes only far depth and leaves opaque G-buffer stores intact", "[almetal][modern-sky][modern-sky-depth]")
{
    @autoreleasepool {
        Fixture f;
        for (bool reverse : {false,true}) for (bool normal_hdr : {false,true}) {
            CAPTURE(reverse,normal_hdr);
            auto g = f.gbuffer(true,reverse,normal_hdr); f.stampStencil(*g); auto output = f.lit(*g,reverse);
            const auto before = f.colors(*output->colorTexture(0));
            std::array<std::vector<std::uint8_t>,4> stores;
            for(unsigned a=0;a<4;++a) stores[a]=f.bytes(*g->colorTexture(a));
            const auto depth = f.read<float>(*g->depthStencilTexture());
            const auto stencil = f.stencil(*g->depthStencilTexture());
            auto scene = f.scene(128,reverse); scene.hdri_texture = f.flat({.25f,.5f,1.f,1.f}); scene.constants.transfer[0] = 2.f;
            REQUIRE(f.renderer->compositeModernSky(*output,*g->depthStencilTexture(),scene,&f.error));
            const auto actual = f.colors(*output->colorTexture(0));
            for (unsigned y=0;y<SIDE;++y) for (unsigned x=0;x<SIDE;++x) {
                const unsigned i=y*SIDE+x;
                if (x<SIDE/2) CHECK(actual[i]==before[i]);
                else { CHECK(actual[i][0]==.5f); CHECK(actual[i][1]==1.f); CHECK(actual[i][2]==2.f); CHECK(actual[i][3]==0.f); }
            }
            for(unsigned a=0;a<4;++a) CHECK(f.bytes(*g->colorTexture(a))==stores[a]);
            CHECK(f.read<float>(*g->depthStencilTexture())==depth);
            CHECK(f.stencil(*g->depthStencilTexture())==stencil);
        }
    }
}
TEST_CASE("Native zero-star configured views composite the dome over black without changing geometry", "[almetal][modern-sky][modern-sky-empty-stars]")
{
    @autoreleasepool {
        Fixture f;
        for(bool emissive:{false,true}) for(bool hdr:{false,true}) for(bool reverse:{false,true}) {
            CAPTURE(emissive,hdr,reverse);
            auto g=f.gbuffer(true,reverse,true,hdr,emissive?3:0); f.stampStencil(*g);
            auto output=f.lit(*g,reverse); auto scene=f.scene(0,reverse);
            REQUIRE(f.renderer->compositeModernSky(*output,*g->depthStencilTexture(),scene,&f.error));
            const auto dome=f.colors(*output->colorTexture(0));
            std::array<std::vector<std::uint8_t>,4> stores;
            for(unsigned a=0;a<4;++a) stores[a]=f.bytes(*g->colorTexture(a));
            const auto depth=f.read<float>(*g->depthStencilTexture());
            const auto stencil=f.stencil(*g->depthStencilTexture());
            scene.constants.config[0]=1;scene.constants.config[1]=emissive;
            scene.stars_texture=g->colorTexture(emissive?3:0);
            // The producer intentionally submits no star draw while its async field is empty.
            REQUIRE(f.renderer->compositeModernSky(*output,*g->depthStencilTexture(),scene,&f.error));
            CHECK(f.colors(*output->colorTexture(0))==dome);
            for(unsigned a=0;a<4;++a) CHECK(f.bytes(*g->colorTexture(a))==stores[a]);
            CHECK(f.read<float>(*g->depthStencilTexture())==depth);
            CHECK(f.stencil(*g->depthStencilTexture())==stencil);
        }
    }
}
TEST_CASE("Native packed point and quad stars preserve unrelated stores and respect opaque depth", "[almetal][modern-sky][modern-sky-stars]")
{
    @autoreleasepool {
        Fixture f;
        struct alignas(16) Star {float x,y,z;std::uint32_t rgba;};
        for(bool emissive:{false,true}) for(bool hdr:{false,true}) for(bool reverse:{false,true}) for(bool quads:{false,true}) {
            CAPTURE(emissive,hdr,reverse,quads);
            std::array<Star,12> stars;
            for(unsigned i=0;i<stars.size();++i) stars[i]={i<6?-.6f:.6f,0.f,1.f,0x80ffffffu};
            if(!quads) stars[1]=stars[6];
            auto g=f.gbuffer(true,reverse,true,hdr,emissive?3:0); f.stampStencil(*g);
            std::array<std::vector<std::uint8_t>,4> before;
            for(unsigned a=0;a<4;++a) before[a]=f.bytes(*g->colorTexture(a));
            const auto depth=f.read<float>(*g->depthStencilTexture());const auto stencil=f.stencil(*g->depthStencilTexture());
            ALMetalWorldModernSkyStarDraw draw{0,quads?12u:2u,quads};
            ALMetalWorldModernSkyStars scene;scene.vertices=f.buffer(stars);scene.vertex_count=stars.size();
            scene.draws=&draw;scene.draw_count=1;scene.star_alpha=1.f;scene.time=3.f;
            scene.width=scene.height=SIDE;scene.emissive=emissive;scene.hdr=hdr;scene.reverse_z=reverse;
            identity(scene.modelview_projection);
            REQUIRE(f.renderer->renderModernSkyStars(*g,scene,&f.error));
            const unsigned selected=emissive?3:0;
            for(unsigned a=0;a<4;++a) if(a!=selected) CHECK(f.bytes(*g->colorTexture(a))==before[a]);
            const auto after=f.bytes(*g->colorTexture(selected));const auto stride=g->colorTexture(selected)->bytesPerBlock();
            unsigned changed=0;
            for(unsigned y=0;y<SIDE;++y) for(unsigned x=0;x<SIDE;++x) {
                const unsigned at=(y*SIDE+x)*stride;
                const bool same=std::equal(after.begin()+at,after.begin()+at+stride,before[selected].begin()+at);
                if(x<SIDE/2) CHECK(same); else changed+=!same;
            }
            CHECK(changed>0);
            CHECK(f.read<float>(*g->depthStencilTexture())==depth);CHECK(f.stencil(*g->depthStencilTexture())==stencil);
            auto masked=f.gbuffer(true,reverse,true,hdr,emissive?3:0);
            const auto masked_before=f.bytes(*masked->colorTexture(selected));
            scene.moon_texture=f.flat({1,1,1,1});scene.moon_quad_center[0]=.6f;scene.moon_quad_center[2]=1.f;
            scene.moon_quad_u[0]=scene.moon_quad_v[1]=1.f;
            REQUIRE(f.renderer->renderModernSkyStars(*masked,scene,&f.error));
            CHECK(f.bytes(*masked->colorTexture(selected))==masked_before);
        }
    }
}
TEST_CASE("Native ray HDRI retains full finite range after exposure and classic gain", "[almetal][modern-sky][modern-sky-hdr]")
{
    @autoreleasepool {
        Fixture f;
        for (float texel : {-1.f, .25f, 65504.f, NAN, INFINITY}) for (float gain : {1.f,1.1f}) {
            CAPTURE(texel,gain); auto scene=f.scene(128);scene.hdri_texture=f.flat({texel,texel,texel,1.f});
            scene.constants.transfer[0]=texel>1.f?1000.f:1.f;scene.constants.transfer[3]=gain;
            const auto pixels=f.draw(scene);
            const float expected=std::isnan(texel)?0.f:std::min(std::clamp(texel*scene.constants.transfer[0],0.f,65024.f)*gain,65504.f);
            for(const auto& pixel:pixels) {
                for(unsigned c=0;c<3;++c) {CHECK(std::isfinite(pixel[c]));CHECK(pixel[c]>=0.f);CHECK(pixel[c]<=65504.f);CHECK(std::abs(pixel[c]-expected)<=std::max(.0005f,expected/2048.f));}
                CHECK(pixel[3]==0.f);
            }
        }
    }
}
TEST_CASE("Native ray dome does not read inactive rainbow or halo contents", "[almetal][modern-sky][modern-sky-optics]")
{
    @autoreleasepool {
        Fixture f;auto scene=f.scene();const auto ordinary=f.draw(scene);
        scene.rainbow_texture=f.flat({NAN,NAN,NAN,1});scene.halo_texture=f.flat({NAN,NAN,NAN,1});
        CHECK(bool(f.draw(scene)==ordinary));
    }
}
TEST_CASE("Native baked cube faces sample the production ray dome and every mip averages its parent", "[almetal][modern-sky][modern-sky-cube-bake]")
{
    @autoreleasepool {
        Fixture f;auto scene=f.scene();auto cube=f.renderer->bakeModernSkyCube(scene,&f.error);INFO(f.error);REQUIRE(cube);
        // Independent final LLCubeMap face basis, in dome (north,up,east) coordinates.
        constexpr float forward[6][3]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
        constexpr float right[6][3]={{0,-1,0},{0,1,0},{0,0,1},{0,0,1},{0,0,1},{0,0,-1}};
        constexpr float up[6][3]={{-1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{-1,0,0},{-1,0,0}};
        for(unsigned face=0;face<6;++face) {
            const auto pixels=f.read<std::array<std::uint8_t,4>>(*cube,0,face);
            for(unsigned y:{0u,31u,63u}) for(unsigned x:{0u,31u,63u}) {
                CAPTURE(face,x,y);auto reference=scene;
                std::fill(std::begin(reference.constants.inverse_projection),std::end(reference.constants.inverse_projection),0.f);
                for(unsigned c=0;c<3;++c) reference.constants.inverse_projection[12+c]=forward[face][c]+(2.f*(x+.5f)/64.f-1.f)*right[face][c]+(2.f*(y+.5f)/64.f-1.f)*up[face][c];
                reference.constants.inverse_projection[15]=1.f;
                const auto ray=f.draw(reference)[0];const auto& pixel=pixels[y*64+x];
                for(unsigned c=0;c<3;++c) {
                    const float encoded=ray[c]<=.0031308f?12.92f*ray[c]:1.055f*std::pow(ray[c],1.f/2.4f)-.055f;
                    CHECK(std::abs(float(pixel[c])-std::clamp(encoded*.5f,0.f,1.f)*255.f)<1.1f);
                }
                CHECK(pixel[3]==255);
            }
            auto previous=pixels;
            for(unsigned mip=1;mip<7;++mip) {
                const unsigned size=64u>>mip;const auto actual=f.read<std::array<std::uint8_t,4>>(*cube,mip,face);
                for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) for(unsigned c=0;c<4;++c) {
                    unsigned sum=0;for(unsigned dy=0;dy<2;++dy) for(unsigned dx=0;dx<2;++dx) sum+=previous[(y*2+dy)*size*2+x*2+dx][c];
                    CHECK(std::abs(float(actual[y*size+x][c])-sum*.25f)<=1.f);
                }
                previous=actual;
            }
        }
    }
}
TEST_CASE("Native probe-less sky cube is neutral at every face and mip before its first bake", "[almetal][modern-sky][modern-sky-cube]")
{
    @autoreleasepool {
        Fixture f;auto cube=f.renderer->createNeutralModernSkyCube(&f.error);INFO(f.error);REQUIRE(cube);
        REQUIRE(cube->descriptor().type==ALMetalTextureType::texture_cube);REQUIRE(cube->width()==64);REQUIRE(cube->descriptor().mipmap_level_count==7);
        for(unsigned mip=0;mip<7;++mip) for(unsigned face=0;face<6;++face) {
            CAPTURE(mip,face);const auto pixels=f.read<std::array<std::uint8_t,4>>(*cube,mip,face);
            const std::array<std::uint8_t,4> expected{64,64,64,255};for(const auto& pixel:pixels) CHECK(pixel==expected);
        }
    }
}
TEST_CASE("Native ray layers obey final body cloud and HDRI precedence", "[almetal][modern-sky][modern-sky-layers]")
{
    @autoreleasepool {
        Fixture f;auto scene=f.scene();
        // All rays look above the horizon; constant maps isolate blend and layer ownership.
        std::fill(std::begin(scene.constants.inverse_projection),std::end(scene.constants.inverse_projection),0.f);
        scene.constants.inverse_projection[13]=1.f;scene.constants.inverse_projection[14]=.5f;scene.constants.inverse_projection[15]=1.f;
        for(auto* quad:{scene.constants.sun_quad,scene.constants.moon_quad,scene.constants.bloom_quad}) {
            quad[0][2]=1.f;quad[1][0]=.25f;quad[2][1]=.25f;
        }
        scene.stars_texture=f.flat({.1f,.2f,.3f,1});
        scene.sun_texture=f.flat({.8f,.2f,.1f,1});scene.sun_next_texture=f.flat({.1f,.3f,.9f,1});
        scene.moon_texture=f.flat({.2f,.7f,.3f,1});
        scene.bloom_texture=f.flat({.1f,.4f,.8f,1});scene.bloom_next_texture=f.flat({.9f,.2f,.1f,1});
        scene.cloud_texture=f.flat({1,1,1,1});scene.hdri_texture=f.flat({.3f,.4f,.5f,1});
        scene.constants.body[1]=scene.constants.body[2]=1.f;
        scene.constants.bloom_color[0]=scene.constants.bloom_color[1]=scene.constants.bloom_color[2]=1.f;
        const auto dome=f.draw(scene);
        scene.stars_texture=f.flat({65504,65504,65504,1});CHECK(f.draw(scene)==dome);
        scene.constants.config[0]=4;const auto sun=f.draw(scene);
        scene.constants.config[0]=1|2|4;CHECK(f.draw(scene)==sun);
        scene.constants.config[0]=8;const auto moon=f.draw(scene);
        scene.constants.config[0]=1|2|4|8;CHECK(f.draw(scene)==moon);CHECK(moon!=sun);
        scene.constants.config[0]=4;scene.constants.body[0]=1.f;const auto next_sun=f.draw(scene);
        scene.sun_texture=scene.sun_next_texture;scene.constants.body[0]=0.f;CHECK(f.draw(scene)==next_sun);CHECK(next_sun!=sun);
        scene.constants.config[0]=128;scene.constants.transfer[1]=1.f;const auto hdri=f.draw(scene);
        scene.constants.config[0]=255;CHECK(f.draw(scene)==hdri);
        scene.constants.config[0]=64;auto& c=scene.constants.clouds;
        c.parameters[0]=1.f;c.light_density[1]=1.f;c.light_density[3]=.001f;c.glow[0]=1.f;c.glow[2]=-1.f;c.glow[3]=1000.f;
        c.cloud_color_shadow[0]=.3f;c.cloud_color_shadow[1]=.4f;c.cloud_color_shadow[2]=.5f;c.cloud_color_shadow[3]=1.f;
        c.cloud_pos_density1[2]=1.f;
        for(unsigned i=0;i<3;++i){c.atmos_ambient[i]=.5f;c.haze_extinction[i]=.5f;c.blue_weight[i]=1.f;c.haze_weight[i]=1.f;}
        const auto clouds=f.draw(scene);CHECK(clouds!=dome);
        scene.constants.config[0]=127;scene.constants.aurora[0]=10.f;scene.constants.aurora[1]=3.f;
        scene.constants.config[3]=1;scene.meteor_data[0][2]=1.f;scene.meteor_data[1][1]=1.f;
        scene.meteor_data[1][2]=1.f;scene.meteor_data[1][3]=.3f;
        scene.meteor_data[2][0]=scene.meteor_data[2][1]=scene.meteor_data[2][2]=100.f;
        CHECK(f.draw(scene)==clouds);
        scene.constants.clouds.parameters[0]=0.f;const auto transparent=f.draw(scene);
        scene.constants.config[0]=63;CHECK(f.draw(scene)==transparent);CHECK(transparent!=clouds);
    }
}
TEST_CASE("Native exposure meters completed water depth while retaining opaque normal provenance", "[almetal][modern-sky][modern-sky-water-meter]")
{
    @autoreleasepool {
        Fixture f;
        for(bool reverse:{false,true}) for(bool normal_hdr:{false,true}) {
            CAPTURE(reverse,normal_hdr);auto g=f.gbuffer(true,reverse,normal_hdr);auto lighting=f.lit(*g,reverse);
            const auto original_depth=f.read<float>(*g->depthStencilTexture());const auto original_normal=f.bytes(*g->colorTexture(2));
            auto completed=f.water(*g,*lighting,reverse);
            REQUIRE(alMetalHasUsage(completed->depthStencilTexture()->descriptor().usage,ALMetalTextureUsage::shader_read));
            const auto final_depth=f.read<float>(*completed->depthStencilTexture());const auto stencil=f.stencil(*completed->depthStencilTexture());
            CHECK(f.bytes(*g->colorTexture(2))==original_normal);CHECK(f.read<float>(*g->depthStencilTexture())==original_depth);
            ALMetalWorldExposureScene scene;scene.source=completed.get();scene.normal=g->colorTexture(2);
            scene.depth=completed->depthStencilTexture();scene.reverse_z=reverse;
            auto no_map=f.renderer->renderExposureMeter(scene,&f.error);INFO(f.error);REQUIRE(no_map);
            scene.water_stencil=completed.get();auto with_map=f.renderer->renderExposureMeter(scene,&f.error);INFO(f.error);REQUIRE(with_map);
            const auto without=f.colors(*no_map->colorTexture(0));const auto with=f.colors(*with_map->colorTexture(0));
            std::array<unsigned,4> groups{};
            for(unsigned y:{8u,24u,48u,64u}) for(unsigned x:{16u,48u,80u,112u}) {
                const unsigned source_y=unsigned((y+.5f)*SIDE/72.f),source_x=unsigned((x+.5f)*SIDE/128.f),at=source_y*SIDE+source_x;
                const bool original_sky=reverse?original_depth[at]<=0.f:original_depth[at]>=1.f;
                const bool far=reverse?final_depth[at]<=0.f:final_depth[at]>=1.f;
                const bool water=(stencil[at]&2u)!=0;
                ++groups[(water?2:0)+(original_sky?1:0)];
                CAPTURE(x,y,source_x,source_y,water,original_sky,far);
                CHECK(without[y*128+x][2]==float(far));
                CHECK(with[y*128+x][2]==float(far||(water&&original_sky)));
            }
            for(unsigned count:groups) CHECK(count>0);
        }
    }
}
TEST_CASE("Native water haze converges to the shared unclamped ray dome only on water", "[almetal][modern-sky][modern-sky-water-dome]")
{
    @autoreleasepool {
        Fixture f;auto g=f.gbuffer(true);auto lighting=f.lit(*g);
        auto sky=f.scene();
        for(unsigned c=0;c<3;++c) sky.constants.dome.sky_ambient[c]=sky.constants.dome.sky_ambient_below[c]=20.f;
        auto baseline=f.water(*g,*lighting,false);auto converged=f.water(*g,*lighting,false,&sky);
        const auto before=f.colors(*baseline->colorTexture(0));const auto after=f.colors(*converged->colorTexture(0));
        const auto stencil=f.stencil(*converged->depthStencilTexture());const auto depth=f.read<float>(*converged->depthStencilTexture());
        unsigned changed=0,unchanged=0,bright=0;
        for(unsigned y=0;y<SIDE;++y) for(unsigned x=0;x<SIDE;++x) {
            const unsigned at=y*SIDE+x;const bool water=(stencil[at]&2u)!=0;
            if(!water) {CHECK(after[at]==before[at]);++unchanged;continue;}
            changed+=after[at]!=before[at];bright+=after[at][0]>1.f;
            // Same final depth reconstruction as the production owner, then a real main-ray GPU evaluation.
            if((x==8||x==24)&&(y==8||y==24)) {
                auto reference=sky;std::fill(std::begin(reference.constants.inverse_projection),std::end(reference.constants.inverse_projection),0.f);
                reference.constants.inverse_projection[12]=2.f*(x+.5f)/SIDE-1.f;
                reference.constants.inverse_projection[13]=2.f*(y+.5f)/SIDE-1.f;
                reference.constants.inverse_projection[14]=depth[at]*2.f-1.f;reference.constants.inverse_projection[15]=1.f;
                const auto ray=f.draw(reference)[0];for(unsigned c=0;c<3;++c) CHECK(std::abs(after[at][c]-ray[c])<std::max(.003f,ray[c]*.003f));
            }
        }
        CHECK(changed>0);CHECK(unchanged>0);CHECK(bright>0);
    }
}
TEST_CASE("Native HDRI split preserves ray dome pixels on its opposite side", "[almetal][modern-sky][modern-sky-split]")
{
    @autoreleasepool {
        Fixture f;auto scene=f.scene(128);scene.hdri_texture=f.flat({2.f,.5f,.25f,1.f});
        scene.constants.transfer[0]=2.f;scene.constants.transfer[1]=0.f;scene.constants.transfer[2]=3.f;scene.constants.transfer[3]=1.1f;
        const auto dome=f.draw(scene);scene.constants.transfer[1]=1.f;const auto hdri=f.draw(scene);
        scene.constants.transfer[1]=.5f;const auto split=f.draw(scene);
        CHECK(bool(dome!=hdri));
        for(unsigned y=0;y<SIDE;++y) for(unsigned x=0;x<SIDE;++x) CHECK(split[y*SIDE+x]==(x<SIDE/2?hdri[y*SIDE+x]:dome[y*SIDE+x]));
    }
}
TEST_CASE("Native packed meteor boundary and procedural aurora produce finite night layers", "[almetal][modern-sky][modern-sky-night]")
{
    @autoreleasepool {
        Fixture f;auto scene=f.scene(16);const auto absent=f.draw(scene);
        scene.constants.config[3]=64;
        for(unsigned i=0;i<64;++i) {
            scene.meteor_data[i*3][2]=1.f;scene.meteor_data[i*3+1][1]=1.f;
            scene.meteor_data[i*3+1][2]=1.f;scene.meteor_data[i*3+1][3]=.3f;
            scene.meteor_data[i*3+2][0]=1.f;scene.meteor_data[i*3+2][1]=.5f;scene.meteor_data[i*3+2][2]=.25f;
        }
        const auto maximum=f.draw(scene);CHECK(bool(maximum!=absent));
        for(const auto& pixel:maximum) for(float channel:pixel) {CHECK(std::isfinite(channel));CHECK(channel>=0.f);CHECK(channel<=65504.f);}
        scene.constants.config[0]=32;scene.constants.config[3]=0;
        scene.constants.aurora[0]=0.f;const auto no_aurora=f.draw(scene);
        scene.constants.aurora[0]=1.f;scene.constants.aurora[1]=13.f;const auto aurora=f.draw(scene);
        CHECK(bool(aurora!=no_aurora));
        for(const auto& pixel:aurora) for(float channel:pixel) {CHECK(std::isfinite(channel));CHECK(channel>=0.f);}
    }
}
