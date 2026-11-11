/**
 * @file almetal_shader_reload_test.mm
 * @brief Native shader rebuilds preserve live render-target ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../almetalshadowmaterials.h"
#include "../almetaluirenderer.h"
#include "../almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <vector>

namespace
{
// Original1eb44 clearShaderCache + program rebuild replaced executable shader
// owners while render targets/history stayed live. This native mechanism has
// no GL equivalent: compile/load a replacement MTLLibrary and immutable PSOs,
// then publish only a complete replacement between command transactions.
std::vector<std::uint8_t> pixels(const ALMetalDevice& device,
                                const ALMetalTexture& texture)
{
    std::string error;
    std::vector<std::uint8_t> result(texture.descriptor().width *
                                     texture.descriptor().height * 4);
    REQUIRE(texture.readback(device, 0, 0, {}, result.data(),
                            texture.descriptor().width * 4, &error));
    INFO(error);
    return result;
}
}

TEST_CASE("World shader rebuild keeps existing targets and runtime sampling policy",
          "[almetal][shader-reload]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto world = ALMetalWorldGBufferRenderer::create(*device, 4, &error);
        INFO(error); REQUIRE(world);
        auto data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(data);
        data->beginFrame(17); world->setFrameData(data);
        CHECK(world->shaderGeneration() == 1);

        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 2;
        descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        auto source = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(source);
        constexpr std::array<std::uint8_t, 16> authored{
            255,0,0,255, 0,255,0,255, 0,0,255,255, 64,128,192,255};
        auto upload = device->makeCommandBuffer("Shader reload input", &error); REQUIRE(upload);
        REQUIRE(source->upload(*upload, 0, 0, {}, authored.data(), 8, &error));
        REQUIRE(upload->commitAndWait(&error));

        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error); REQUIRE(library);
        auto* old_materials = data->shadowMaterials(*library, &error); REQUIRE(old_materials);
        ALMetalSamplerDescriptor sampler_descriptor;
        sampler_descriptor.support_argument_buffers = true;
        auto sampler = ALMetalSamplerState::create(*device, sampler_descriptor, &error);
        REQUIRE(sampler);
        const std::array<const ALMetalTexture*, 2> textures{source.get(), source.get()};
        const ALMetalShadowMaterialRequest request{textures.data(), 2, sampler.get()};
        auto material_command = device->makeCommandBuffer("Pre-reload material lease", &error);
        REQUIRE(material_command);
        auto old_pass = old_materials->prepare(*material_command,
            std::span<const ALMetalShadowMaterialRequest>(&request, 1), &error);
        INFO(error); REQUIRE(old_pass);
        const bool indirect = old_pass->usesArgumentBuffer(0);
        ALMetalWorldDataSlice data_slice;
        REQUIRE(data->publish(*material_command, authored.data(), authored.size(),
                              data_slice, &error));
        const auto arena = data->stats();

        REQUIRE(world->beginCommandBatch("World before shader reload", &error));
        CHECK_FALSE(world->reloadShaders(&error));
        CHECK(world->shaderGeneration() == 1);
        CHECK(world->commandBatchActive());
        ALMetalWorldDebugBufferScene scene;
        scene.source = source.get(); scene.width = scene.height = 2;
        scene.output_format = ALMetalPixelFormat::rgba8_unorm;
        auto retained = world->renderDebugBuffer(scene, &error); INFO(error); REQUIRE(retained);
        REQUIRE(world->commitCommandBatch(&error));
        const auto pool = world->renderTargetPoolStats();
        const auto* retained_identity = retained->colorTexture(0);
        // Do not wait for the old command before replacing its pipeline owner.
        REQUIRE(world->reloadShaders(&error)); INFO(error);
        CHECK(world->shaderGeneration() == 2);
        CHECK(world->anisotropy() == 4);
        CHECK(world->renderTargetPoolStats().acquisitions == pool.acquisitions);
        CHECK(data->shadowMaterials(*library, &error) != old_materials);
        CHECK(data->stats().published_bytes == arena.published_bytes);
        CHECK(old_pass->requestCount() == 1);
        CHECK(old_pass->usesArgumentBuffer(0) == indirect);
        REQUIRE(material_command->commitAndWait(&error));
        CHECK(retained->colorTexture(0) == retained_identity);
        CHECK(pixels(*device, *retained_identity) ==
              std::vector<std::uint8_t>(authored.begin(), authored.end()));
        scene.source = retained_identity;
        auto continued = world->renderDebugBuffer(scene, &error); INFO(error); REQUIRE(continued);
        CHECK(pixels(*device, *continued->colorTexture(0)) ==
              std::vector<std::uint8_t>(authored.begin(), authored.end()));
        CHECK(world->renderTargetPoolStats().acquisitions == pool.acquisitions + 1);
    }
}

TEST_CASE("UI shader rebuild preserves output backing and rejects open frames",
          "[almetal][shader-reload]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto frames = ALMetalFrameScheduler::create(*device, 2, &error); REQUIRE(frames);
        auto ui = ALMetalUIRenderer::createForTexture(*device, *frames,
            ALMetalPixelFormat::rgba8_unorm, &error); INFO(error); REQUIRE(ui);
        CHECK(ui->shaderGeneration() == 1);
        constexpr std::array<std::array<float, 4>, 3> positions{{
            {-1,-1,0,1}, {3,-1,0,1}, {-1,3,0,1}}};
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(positions); descriptor.storage = ALMetalStorageMode::shared;
        auto vertices = ALMetalBuffer::create(*device, descriptor, &error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, positions.data(), sizeof(positions), &error));
        ALMetalUIBatch batch;
        batch.vertices = vertices.get(); batch.vertex_count = 3;
        batch.target_width = batch.target_height = 4;
        batch.viewport = {0,0,4,4,0,1}; batch.scissor = {0,0,4,4};
        batch.shader = ALMetalUIShader::uniform_color; batch.blend.enabled = false;
        batch.solid_color[0] = .25f; batch.solid_color[1] = .5f;
        batch.solid_color[2] = .75f; batch.solid_color[3] = 1.f;
        for (unsigned axis = 0; axis < 4; ++axis)
            batch.matrices.modelview_projection[axis * 4 + axis] = 1.f;
        REQUIRE(ui->draw(batch, &error));
        CHECK_FALSE(ui->reloadShaders(&error));
        CHECK(ui->hasOpenFrame()); CHECK(ui->shaderGeneration() == 1);
        REQUIRE(ui->endFrame(&error));
        const auto before = ui->stats();
        const auto* retained = ui->outputTexture(); REQUIRE(retained);
        REQUIRE(ui->reloadShaders(&error)); INFO(error);
        CHECK(ui->shaderGeneration() == 2);
        CHECK(ui->outputTexture() == retained);
        CHECK(ui->stats().frames == before.frames);
        CHECK(ui->stats().batches == before.batches);
        CHECK(ui->stats().pipeline_states == before.pipeline_states);
        const auto first = pixels(*device, *retained);
        REQUIRE(first.size() == 64);
        CHECK(first[0] == 64); CHECK(first[1] == 128);
        CHECK(first[2] == 191); CHECK(first[3] == 255);
        REQUIRE(ui->draw(batch, &error)); REQUIRE(ui->endFrame(&error));
        CHECK(pixels(*device, *ui->outputTexture()) == first);
        auto handed_off = ui->takeOutputTexture(); REQUIRE(handed_off);
        REQUIRE(ui->setReusableOutputTexture(handed_off, &error));
        CHECK_FALSE(handed_off);
        REQUIRE(ui->reloadShaders(&error));
        CHECK(ui->shaderGeneration() == 3);
        REQUIRE(ui->draw(batch, &error)); REQUIRE(ui->endFrame(&error));
        CHECK(pixels(*device, *ui->outputTexture()) == first);
        REQUIRE(frames->waitUntilIdle(5000, &error));
    }
}
