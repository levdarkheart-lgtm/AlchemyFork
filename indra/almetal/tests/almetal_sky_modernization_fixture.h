/**
 * @file almetal_sky_modernization_fixture.h
 * @brief Real production Metal resources for the ray-sky regressions.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#pragma once
#include "almetalworld.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace sky_test
{
using Color = std::array<float, 4>;
using Image = std::vector<Color>;
constexpr unsigned SIDE = 32;
inline void identity(float* values)
{ for (unsigned i = 0; i < 16; ++i) values[i] = i % 5 == 0 ? 1.f : 0.f; }
inline float half(std::uint16_t bits)
{ __fp16 value; std::memcpy(&value, &bits, 2); return value; }
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::vector<std::unique_ptr<ALMetalTexture>> maps;
    std::vector<std::unique_ptr<ALMetalBuffer>> buffers;
    const ALMetalTexture* legacy_environment = nullptr;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        INFO(device->info().name); REQUIRE(device->info().unified_memory);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error); INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        arena->beginFrame(1); renderer->setFrameData(std::move(arena));
    }
    ~Fixture() { if (device) CHECK(device->waitUntilIdle(&error)); }
    const ALMetalTexture* flat(Color value)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 1; descriptor.format = ALMetalPixelFormat::rgba16_float;
        auto texture = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(texture);
        std::array<__fp16, 4> data; std::copy(value.begin(), value.end(), data.begin());
        auto command = device->makeCommandBuffer("Authored sky test texture", &error); REQUIRE(command);
        REQUIRE(texture->upload(*command, 0, 0, {}, data.data(), 8, &error));
        REQUIRE(command->commitAndWait(&error));
        maps.push_back(std::move(texture)); return maps.back().get();
    }
    template<class T> ALMetalBuffer* buffer(const T& data)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(data); descriptor.storage = ALMetalStorageMode::shared;
        auto result = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(result);
        REQUIRE(result->write(0, &data, sizeof(data), &error));
        buffers.push_back(std::move(result)); return buffers.back().get();
    }
    std::unique_ptr<ALMetalRenderTarget> gbuffer(bool world, bool reverse = false, bool normal_hdr = true, bool hdr = true,
                                               int star_attachment = -1)
    {
        struct alignas(16) Streams
        {
            float positions[4][4]{{-1,-1,0,0},{0,-1,0,0},{-1,1,0,0},{0,1,0,0}};
            float normals[4][4]{};
            float uv[4][2]{};
            std::uint8_t color[4][4]{};
        } data;
        for (unsigned i = 0; i < 4; ++i)
        {
            data.positions[i][2] = reverse ? .75f : .25f;
            data.normals[i][2] = 1.f; data.uv[i][0] = data.uv[i][1] = .5f;
            std::fill(std::begin(data.color[i]), std::end(data.color[i]), 255);
        }
        const std::array<std::uint16_t, 6> indices{0,1,2,2,1,3};
        ALMetalWorldSimpleBatch batch;
        batch.vertices = buffer(data); batch.indices = buffer(indices);
        batch.vertex_count = 4; batch.index_count = 6;
        batch.normal_offset = offsetof(Streams, normals); batch.texcoord_offset = offsetof(Streams, uv);
        batch.color_offset = offsetof(Streams, color); batch.double_sided = true;
        batch.diffuse_textures[0] = flat({.125f,.25f,.375f,1.f}); batch.texture_count = 1;
        identity(batch.modelview_projection); identity(batch.modelview);
        for (unsigned axis = 0; axis < 3; ++axis) batch.normal_matrix[axis][axis] = 1.f;
        ALMetalWorldGBufferScene scene;
        scene.width = scene.height = SIDE; scene.hdr = hdr; scene.normal_hdr = normal_hdr; scene.reverse_z = reverse;
        scene.sky_star_attachment = star_attachment;
        if (world) { scene.batches = &batch; scene.batch_count = 1; }
        auto result = renderer->render(scene, &error); INFO(error); REQUIRE(result); return result;
    }
    std::unique_ptr<ALMetalRenderTarget> lit(const ALMetalRenderTarget& gbuffer, bool reverse = false)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE; descriptor.format = ALMetalPixelFormat::rg8_unorm;
        auto light = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(light);
        std::array<std::uint8_t, SIDE * SIDE * 2> pixels; pixels.fill(255);
        auto command = device->makeCommandBuffer("Sky fixture light map", &error); REQUIRE(command);
        REQUIRE(light->upload(*command, 0, 0, {}, pixels.data(), SIDE * 2, &error));
        REQUIRE(command->commitAndWait(&error));
        if (!legacy_environment)
        {
            maps.push_back(renderer->createNeutralModernSkyCube(&error)); REQUIRE(maps.back());
            legacy_environment = maps.back().get();
        }
        ALMetalWorldBaseLightingScene scene; scene.gbuffer = &gbuffer; scene.light_map = light.get();
        scene.legacy_environment = legacy_environment;
        scene.constants.environment_config[0] = 1;
        for (unsigned axis = 0; axis < 3; ++axis) scene.constants.environment_matrix[axis][axis] = 1.f;
        identity(scene.constants.inverse_projection); scene.constants.light_direction[2] = 1.f;
        scene.constants.surface[0] = scene.constants.surface[1] = 1.f;
        scene.constants.config[0] = reverse;
        scene.hdr = gbuffer.descriptor().colors[3].format == ALMetalPixelFormat::rg11b10_float;
        auto result = renderer->renderBaseLighting(scene, &error); INFO(error); REQUIRE(result);
        maps.push_back(std::move(light)); return result;
    }
    ALMetalWorldModernSkyScene scene(unsigned layers = 0, bool reverse = false)
    {
        ALMetalWorldModernSkyScene result; result.width = result.height = SIDE;
        auto& c = result.constants; identity(c.inverse_projection);
        for (unsigned axis = 0; axis < 3; ++axis) { c.eye_to_sky[axis][axis] = 1.f; c.hdri_rotation[axis][axis] = 1.f; }
        c.config[0] = layers; c.config[1] = 1; c.config[2] = reverse;
        c.dome.camera_local[3] = 15000.f;
        c.dome.light_density[1] = 1.f; c.dome.light_density[3] = .001f;
        c.dome.blue_horizon[0] = .4f; c.dome.blue_horizon[1] = .3f; c.dome.blue_horizon[2] = .2f; c.dome.blue_horizon[3] = .1f;
        c.dome.glow[0] = 1.f; c.dome.glow[2] = -1.f; c.dome.glow[3] = 1000.f;
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            c.dome.haze_extinction[channel] = .5f; c.dome.blue_weight[channel] = c.dome.haze_weight[channel] = 1.f;
            c.dome.sky_light[channel] = .5f; c.dome.sky_ambient[channel] = .2f;
            c.dome.sky_light_atten[channel] = .1f; c.dome.sky_ambient_below[channel] = .1f;
        }
        c.dome.sky_ambient_below[3] = .2f; c.dome.optics[1] = 10.f;
        return result;
    }
    template<class T> std::vector<T> read(const ALMetalTexture& texture, unsigned mip = 0, unsigned slice = 0)
    {
        const unsigned width = std::max(texture.width() >> mip, 1u), height = std::max(texture.height() >> mip, 1u);
        std::vector<T> result(width * height);
        REQUIRE(texture.readback(*device, mip, slice, {}, result.data(), width * sizeof(T), &error));
        return result;
    }
    Image colors(const ALMetalTexture& texture)
    {
        const auto words = read<std::array<std::uint16_t, 4>>(texture);
        Image result(words.size());
        for (unsigned i = 0; i < words.size(); ++i) for (unsigned c = 0; c < 4; ++c) result[i][c] = half(words[i][c]);
        return result;
    }
    std::vector<std::uint8_t> bytes(const ALMetalTexture& texture)
    {
        const auto row = texture.width() * texture.bytesPerBlock();
        std::vector<std::uint8_t> result(row * texture.height());
        REQUIRE(texture.readback(*device,0,0,{},result.data(),row,&error)); return result;
    }
    // Metal's defined stencil-plane blit avoids treating packed depth/stencil as CPU floats.
    std::vector<std::uint8_t> stencil(const ALMetalTexture& texture)
    {
        const auto row = (texture.width() + 255u) & ~255u;
        auto command = device->makeCommandBuffer("Read preserved sky stencil", &error); REQUIRE(command);
        id<MTLDevice> native = (__bridge id<MTLDevice>)command->nativeDevice();
        id<MTLBuffer> staging = [native newBufferWithLength:row * texture.height() options:MTLResourceStorageModeShared];
        REQUIRE(staging);
        id<MTLCommandBuffer> native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLBlitCommandEncoder> blit = [native_command blitCommandEncoder]; REQUIRE(blit);
        [blit copyFromTexture:(__bridge id<MTLTexture>)texture.nativeHandle()
                 sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                  sourceSize:MTLSizeMake(texture.width(),texture.height(),1)
                    toBuffer:staging destinationOffset:0 destinationBytesPerRow:row
       destinationBytesPerImage:row * texture.height() options:MTLBlitOptionStencilFromDepthStencil];
        [blit endEncoding]; REQUIRE(command->commitAndWait(&error));
        std::vector<std::uint8_t> result(texture.width() * texture.height());
        for (unsigned y=0;y<texture.height();++y)
            std::memcpy(result.data()+y*texture.width(),static_cast<const std::uint8_t*>(staging.contents)+y*row,texture.width());
        return result;
    }
    void stampStencil(ALMetalRenderTarget& target)
    {
        ALMetalRenderPassConfig config;
        for(auto& c:config.colors){c.load=ALMetalLoadAction::load;c.store=ALMetalStoreAction::store;}
        config.depth_load=ALMetalLoadAction::load;config.depth_store=ALMetalStoreAction::store;
        config.stencil_load=ALMetalLoadAction::clear;config.stencil_store=ALMetalStoreAction::store;config.clear_stencil=0x5a;
        auto pass=target.makeRenderPass(config,&error);REQUIRE(pass);
        auto command=device->makeCommandBuffer("Authored nonzero stencil before sky",&error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Stamp sky preservation sentinel",&error);REQUIRE(encoder);
        REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));
    }
    std::unique_ptr<ALMetalRenderTarget> water(const ALMetalRenderTarget& g,
        const ALMetalRenderTarget& lighting,bool reverse,const ALMetalWorldModernSkyScene* sky=nullptr)
    {
        ALMetalTextureDescriptor desc;desc.width=desc.height=1;desc.format=ALMetalPixelFormat::rg32_float;
        auto cells=ALMetalTexture::create(*device,desc,&error);REQUIRE(cells);
        const std::array<float,2> cell{0,1};
        auto upload=device->makeCommandBuffer("Author one real procedural water cell",&error);REQUIRE(upload);
        REQUIRE(cells->upload(*upload,0,0,{},cell.data(),sizeof(cell),&error));REQUIRE(upload->commitAndWait(&error));
        ALMetalWorldWaterSurface surface;
        auto& field=surface.field;field.cell_map=cells.get();field.above_water=true;
        identity(field.modelview_projection);identity(field.inverse_projection);identity(surface.modelview);
        // One cell spans half the view at a depth in front of both ground and sky.
        field.modelview_projection[5]=.5f;field.modelview_projection[13]=-.5f;
        field.modelview_projection[10]=0.f;field.modelview_projection[14]=reverse?.75f:-.75f;
        field.cell_square[0]=field.cell_square[1]=-1.f;field.cell_square[2]=2.f;field.cell_square[3]=1.f;
        field.rings[0]=field.rings[1]=field.rings[2]=1.f;field.water_plane[2]=1.f;field.water_plane[3]=1.f;field.fog[0]=1.f;
        field.draws[0]={0,4,1,true};field.draws[1]={1,9,12,false};field.draws[2]={2,9,4,false};field.draw_count=3;
        surface.normal_maps[0]=surface.normal_maps[1]=flat({.5f,.5f,1.f,1.f});
        for(unsigned axis=0;axis<3;++axis)surface.normal_matrix[axis][axis]=1.f;
        surface.eye_position[2]=1.f;surface.water_far[0]=256.f;
        surface.normal_scale_glint[0]=surface.normal_scale_glint[1]=surface.normal_scale_glint[2]=1.f;surface.normal_scale_glint[3]=1.f;
        surface.optics[1]=1.f;surface.optics[2]=.7f;surface.light_direction[2]=1.f;surface.parameters[2]=1.f;
        surface.underwater=false;surface.refracts=false;surface.has_excluded_surface=false;surface.uses_shadows=false;
        ALMetalWorldAlphaEnvironment environment;
        environment.water_plane[2]=1.f;environment.water_plane[3]=1.f;environment.light_direction[2]=1.f;
        environment.water_fog[0]=1.f;environment.water_fog[2]=-1.f;environment.atmospheric[3]=1.f;
        environment.glow_max_y[0]=1.f;environment.glow_max_y[2]=-1.f;environment.glow_max_y[3]=1000.f;
        for(unsigned axis=0;axis<3;++axis)environment.eye_to_sky[axis][axis]=1.f;
        ALMetalWorldLegacyAlphaLighting lighting_constants;
        lighting_constants.sun_direction[2]=1.f;
        ALMetalWorldAtmosphericHaze haze;identity(haze.inverse_projection);haze.sky_behind=sky!=nullptr;haze.water_ground_fade=sky?1.f:0.f;
        std::array<ALMetalWorldSimpleBatch,2> batches;batches[0].water_surface=true;batches[1].atmospheric_haze=true;
        ALMetalWorldOpaquePostDeferredScene scene;
        scene.gbuffer=&g;scene.composed_lighting=&lighting;scene.batches=batches.data();scene.batch_count=sky?2:1;
        scene.width=scene.height=SIDE;scene.reverse_z=reverse;scene.hdr=g.descriptor().colors[3].format==ALMetalPixelFormat::rg11b10_float;
        scene.water_surface=&surface;scene.legacy_environment=legacy_environment;
        scene.alpha_environment=&environment;scene.alpha_lighting=&lighting_constants;scene.exposure_depth=true;
        scene.modern_sky=sky;scene.atmospheric_haze=sky?&haze:nullptr;
        auto result=renderer->renderOpaquePostDeferred(scene,&error);INFO(error);REQUIRE(result);
        maps.push_back(std::move(cells));return result;
    }
    Image draw(const ALMetalWorldModernSkyScene& scene, bool world = false)
    {
        const bool reverse = scene.constants.config[2] != 0;
        auto g = gbuffer(world, reverse); auto output = lit(*g, reverse);
        REQUIRE(renderer->compositeModernSky(*output, *g->depthStencilTexture(), scene, &error));
        return colors(*output->colorTexture(0));
    }
};
}
