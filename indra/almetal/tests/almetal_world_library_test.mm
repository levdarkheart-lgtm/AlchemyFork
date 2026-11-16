/**
 * @file almetal_world_library_test.mm
 * @brief Shared world library epochs and atomic main/probe executable reload.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalshadowmaterials.h"
#include "../almetalworldmaterialdata.h"
#include <cstdlib>
#include <filesystem>

namespace
{
// Original1eb44 forced program rebuilding leaves targets/history owned by the
// pipeline. A native library/argument encoder has no GL equivalent: one exact
// main/probe lease must be staged and published as a coherent owner group.
struct SharedLibraryFixture : Fixture
{
    std::unique_ptr<ALMetalWorldGBufferRenderer> probe;
    ALMetalWorldMaterialDataCache material_cache;
    ALMetalWorldSimpleBatch source;
    std::shared_ptr<ALMetalWorldSourceView> metadata;

    SharedLibraryFixture() : material_cache(*device)
    {
        auto library = data->builtinLibrary(&error); INFO(error); REQUIRE(library);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 4, library, &error);
        INFO(error); REQUIRE(renderer);
        probe = ALMetalWorldGBufferRenderer::create(*device, 4, library, &error);
        INFO(error); REQUIRE(probe);
        renderer->setFrameData(data); probe->setFrameData(data);
        source = batch(1);
        source.texcoord1_offset = source.texcoord2_offset = offsetof(Geometry, uv);
        REQUIRE(ALMetalWorldMaterialSource::publish(*device, source, material_cache, &error));
        REQUIRE(ALMetalWorldResourceSource::publish(*device, source, resources->textures, &error));
        metadata = std::make_shared<ALMetalWorldSourceView>();
        metadata->candidates.push_back({{71, 3, 1}, 0, 0});
        ALMetalWorldSourceRegion region;
        std::memcpy(region.modelview_projection, source.modelview_projection, sizeof(region.modelview_projection));
        std::memcpy(region.modelview, source.modelview, sizeof(region.modelview));
        std::memcpy(region.normal_matrix, source.normal_matrix, sizeof(region.normal_matrix));
        metadata->regions.push_back(region);
    }
    auto owners() { return std::array{renderer.get(), probe.get()}; }
    ALMetalWorldGBufferScene rowScene()
    {
        auto value = scene(); value.normal_hdr = false;
        value.batches = &source; value.batch_count = 1;
        value.source_owner = resources; value.source_rows = metadata;
        return value;
    }
    ALMetalWorldSourceRequest request()
    {
        ALMetalWorldSourceRequest value; value.key = metadata->candidates[0].key;
        value.source = &source; value.native.owner = resources;
        for (const auto& buffer : resources->buffers)
        {
            if (buffer.get() == source.vertices) value.native.vertices = buffer;
            if (buffer.get() == source.indices) value.native.indices = buffer;
        }
        return value;
    }
    struct Encoded
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::unique_ptr<ALMetalCommandBuffer> command;
    };
    Encoded encode(const ALMetalWorldSourceFrameView& view,
                   const ALMetalShaderLibrary& library)
    {
        ALMetalRenderPipelineDescriptor pd; pd.library = &library;
        pd.vertex_function = "almetal_world_material_source_rows_vertex";
        pd.fragment_function = "almetal_world_material_source_rows_fragment";
        pd.color_attachment_count = 4;
        for (auto& attachment : pd.colors) attachment.format = ALMetalPixelFormat::rgba8_unorm;
        pd.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        auto pipeline = ALMetalRenderPipeline::create(*device, pd, &error); INFO(error); REQUIRE(pipeline);
        ALMetalDepthStencilDescriptor dd;
        auto depth = ALMetalDepthStencilState::create(*device, dd, &error); REQUIRE(depth);
        ALMetalRenderTargetDescriptor td; td.width = td.height = side; td.color_attachment_count = 4;
        for (auto& attachment : td.colors) attachment.format = ALMetalPixelFormat::rgba8_unorm;
        td.depth_stencil_format = pd.depth_stencil_format;
        Encoded encoded; encoded.target = ALMetalRenderTarget::create(*device, td, &error); REQUIRE(encoded.target);
        ALMetalRenderPassConfig config; config.depth_store = config.stencil_store = ALMetalStoreAction::store;
        auto pass = encoded.target->makeRenderPass(config, &error); REQUIRE(pass);
        encoded.command = device->makeCommandBuffer("Retained prior world-library source view", &error); REQUIRE(encoded.command);
        auto encoder = ALMetalRenderEncoder::begin(*encoded.command, *pass, "Prior world-library source view", &error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error)); REQUIRE(encoder->setDepthStencilState(depth.get(), &error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1}, &error)); REQUIRE(encoder->setScissor({0,0,side,side}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(view.bind(*encoder, *encoded.command, &error));
        REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle, source.index_count,
            source.index_type, *source.indices, source.index_offset, view.rowId(0), &error));
        REQUIRE(encoder->end(&error));
        return encoded;
    }
};

void awaitRetirement(const std::weak_ptr<const ALMetalWorldSourceFrameView>& view)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!view.expired() && std::chrono::steady_clock::now() < end) std::this_thread::yield();
    CHECK(view.expired());
}
}

TEST_CASE("almetal: main and probe consume one exact library and persistent source context",
          "[almetal][world-library]")
{
    @autoreleasepool
    {
        SharedLibraryFixture f;
        const auto lease = f.data->builtinLibrary(&f.error);
        REQUIRE(lease); CHECK(f.data->builtinLibraryEpoch() == 1);
        CHECK(f.renderer->shaderLibrary() == lease); CHECK(f.probe->shaderLibrary() == lease);
        CHECK(f.renderer->shaderLibrary()->nativeHandle() == f.probe->shaderLibrary()->nativeHandle());
        auto pages = f.data->worldSourcePages(*lease, 4, &f.error); INFO(f.error); REQUIRE(pages);
        CHECK(pages == f.data->worldSourcePages(*f.probe->shaderLibrary(), 4, &f.error));
        auto context = f.data->worldResources(*lease, {}, &f.error); REQUIRE(context);
        CHECK(context == f.data->worldResources(*f.probe->shaderLibrary(), {}, &f.error));
        auto scene = f.rowScene(); scene.source_rows.reset();
        auto scalar = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(scalar);
        const auto expected = f.read(*scalar);
        scene.source_rows = f.metadata;
        auto main = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(main);
        CHECK(f.renderer->worldSourceStats().bound == 1);
        const auto first = pages->stats(); REQUIRE(first.row_publications == 1);
        auto probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(probe);
        CHECK(f.probe->worldSourceStats().bound == 1);
        CHECK(pages->stats().row_publications == first.row_publications);
        CHECK(pages->stats().hits > first.hits);
        CHECK(f.read(*main) == expected); CHECK(f.read(*probe) == expected);

        // Attaching another arena must not replace an already-created PSO or
        // its exact shader lease, even if that arena has acquired another one.
        auto other = ALMetalWorldFrameData::create(*f.device, &f.error); REQUIRE(other);
        auto other_lease = other->builtinLibrary(&f.error); REQUIRE(other_lease);
        REQUIRE(other_lease != lease);
        f.probe->setFrameData(other);
        CHECK(f.probe->shaderLibrary() == lease);
        f.probe->setFrameData(f.data);
    }
}

TEST_CASE("almetal: failed second world shader stage preserves both owners and shared caches",
          "[almetal][world-library][shader-reload]")
{
    @autoreleasepool
    {
        SharedLibraryFixture f; auto owners = f.owners();
        const auto old = f.data->builtinLibrary(&f.error);
        const auto epoch = f.data->builtinLibraryEpoch();
        auto pages = f.data->worldSourcePages(*old, 4, &f.error); REQUIRE(pages);
        auto resources = f.data->worldResources(*old, {}, &f.error); REQUIRE(resources);
        auto* shadows = f.data->shadowMaterials(*old, &f.error); REQUIRE(shadows);
        auto scene = f.rowScene();
        auto main = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(main);
        auto probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(probe);
        REQUIRE(f.renderer->worldSourceStats().bound == 1); REQUIRE(f.probe->worldSourceStats().bound == 1);
        const auto expected = f.read(*main);
        const auto page_stats = pages->stats();
        CHECK_FALSE(f.renderer->reloadShaders(&f.error));
        CHECK_FALSE(f.probe->reloadShaders(&f.error));
        CHECK(f.data->builtinLibraryEpoch() == epoch);
        CHECK(f.renderer->shaderLibrary() == old); CHECK(f.probe->shaderLibrary() == old);
        CHECK(f.data->shadowMaterials(*old, &f.error) == shadows);
        CHECK(f.data->worldSourcePages(*old, 4, &f.error) == pages);
        CHECK(f.data->worldResources(*old, {}, &f.error) == resources);
        f.probe->failNextShaderReloadForTesting();
        CHECK_FALSE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error));
        CHECK_FALSE(f.error.empty());
        CHECK(f.data->builtinLibraryEpoch() == epoch); CHECK(f.data->builtinLibrary(&f.error) == old);
        CHECK(f.renderer->shaderLibrary() == old); CHECK(f.probe->shaderLibrary() == old);
        CHECK(f.renderer->shaderGeneration() == 1); CHECK(f.probe->shaderGeneration() == 1);
        CHECK(f.data->shadowMaterials(*old, &f.error) == shadows);
        CHECK(f.data->worldSourcePages(*old, 4, &f.error) == pages);
        CHECK(f.data->worldResources(*old, {}, &f.error) == resources);
        CHECK(pages->stats().row_publications == page_stats.row_publications);
        auto retained = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(retained);
        CHECK(f.read(*retained) == expected);
        REQUIRE(f.renderer->beginCommandBatch("Shared reload must reject an active owner", &f.error));
        CHECK_FALSE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error));
        CHECK(f.renderer->commandBatchActive()); CHECK(f.data->builtinLibraryEpoch() == epoch);
        f.renderer->cancelCommandBatch();
        REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error)); INFO(f.error);
        const auto fresh = f.data->builtinLibrary(&f.error); REQUIRE(fresh); REQUIRE(fresh != old);
        CHECK(f.data->builtinLibraryEpoch() == epoch + 1);
        CHECK(f.renderer->shaderLibrary() == fresh); CHECK(f.probe->shaderLibrary() == fresh);
        CHECK(f.renderer->shaderGeneration() == 2); CHECK(f.probe->shaderGeneration() == 2);
        CHECK(f.renderer->anisotropy() == 4); CHECK(f.probe->anisotropy() == 4);
        CHECK(f.data->shadowMaterials(*fresh, &f.error) != shadows);
        CHECK(f.data->worldSourcePages(*fresh, 4, &f.error) != pages);
        CHECK(f.data->worldResources(*fresh, {}, &f.error) != resources);
        auto continued = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(continued);
        CHECK(f.read(*continued) == expected); CHECK(f.renderer->worldSourceStats().bound == 2);
        auto continued_probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(continued_probe);
        CHECK(f.read(*continued_probe) == expected); CHECK(f.probe->worldSourceStats().bound == 3);
    }
}

TEST_CASE("almetal: old source-view commands survive shared shader publication and abandonment",
          "[almetal][world-library][shader-reload]")
{
    @autoreleasepool
    {
        SharedLibraryFixture f; auto library = f.data->builtinLibrary(&f.error);
        auto pages = f.data->worldSourcePages(*library, 4, &f.error); REQUIRE(pages);
        const auto request = f.request(); ALMetalWorldSourceGlobals globals;
        auto view = pages->freeze(std::span(&request, 1), f.metadata->regions, globals, &f.error); INFO(f.error); REQUIRE(view);
        auto reference = f.encode(*view, *library);
        REQUIRE(reference.command->commitAndWait(&f.error));
        const auto expected = f.read(*reference.target);
        REQUIRE(std::any_of(expected.colors[0].begin(), expected.colors[0].end(), [](auto value) { return value != 0; }));
        auto pending = f.encode(*view, *library);
        auto abandoned = f.encode(*view, *library);
        std::weak_ptr<const ALMetalWorldSourceFrameView> weak = view;
        view.reset(); REQUIRE_FALSE(weak.expired());
        // These encoders have ended but their native commands have not been
        // submitted. Reload's eager texture setup can complete on the same
        // queue, and old command/resource leases must remain independently live.
        auto owners = f.owners();
        REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error)); INFO(f.error);
        pages.reset(); library.reset();
        abandoned.command.reset(); REQUIRE_FALSE(weak.expired());
        REQUIRE(pending.command->commitAndWait(&f.error));
        CHECK(f.read(*pending.target) == expected);
        pending.command.reset(); awaitRetirement(weak);

        const auto fresh = f.data->builtinLibrary(&f.error);
        auto fresh_pages = f.data->worldSourcePages(*fresh, 4, &f.error); REQUIRE(fresh_pages);
        auto abandoned_view = fresh_pages->freeze(std::span(&request, 1), f.metadata->regions, globals, &f.error); REQUIRE(abandoned_view);
        std::weak_ptr<const ALMetalWorldSourceFrameView> abandoned_weak = abandoned_view;
        auto only_abandoned = f.encode(*abandoned_view, *fresh);
        abandoned_view.reset(); REQUIRE_FALSE(abandoned_weak.expired());
        only_abandoned.command.reset(); awaitRetirement(abandoned_weak);
    }
}

TEST_CASE("almetal: explicit older world library remains private to one renderer",
          "[almetal][world-library][.world-library-previous-shader]")
{
    @autoreleasepool
    {
        const char* path = std::getenv("ALCHEMY_WORLD_RESOURCE_BASELINE_LIBRARY");
        REQUIRE(path); REQUIRE(std::filesystem::is_regular_file(path));
        SharedLibraryFixture f;
        const auto shared = f.data->builtinLibrary(&f.error);
        const auto epoch = f.data->builtinLibraryEpoch();
        auto pages = f.data->worldSourcePages(*shared, 4, &f.error); REQUIRE(pages);
        auto* shadows = f.data->shadowMaterials(*shared, &f.error); REQUIRE(shadows);
        auto scene = f.rowScene(); scene.source_rows.reset();
        auto baseline = f.renderer->render(scene, &f.error); REQUIRE(baseline);
        const auto expected = f.read(*baseline);
        REQUIRE(f.renderer->reloadShaders(std::string(path), &f.error)); INFO(f.error);
        REQUIRE_FALSE(f.renderer->shaderLibrary()->hasFunction("almetal_world_material_source_rows_vertex"));
        CHECK(f.renderer->shaderLibrary() != shared); CHECK(f.probe->shaderLibrary() == shared);
        CHECK(f.data->builtinLibrary(&f.error) == shared); CHECK(f.data->builtinLibraryEpoch() == epoch);
        CHECK(f.data->shadowMaterials(*shared, &f.error) == shadows);
        CHECK(f.data->worldSourcePages(*shared, 4, &f.error) == pages);
        scene.source_rows = f.metadata;
        auto private_old = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(private_old);
        CHECK(f.renderer->worldSourceStats().bound == 0); CHECK(f.renderer->worldSourceStats().fallback == 1);
        CHECK(f.read(*private_old) == expected);
        auto current_probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(current_probe);
        CHECK(f.probe->worldSourceStats().bound == 1); CHECK(f.read(*current_probe) == expected);
    }
}

TEST_CASE("almetal: High shadow specialization preserves a private pre-specialization library",
          "[almetal][world-library][.world-library-high-shadow-previous]")
{
    @autoreleasepool
    {
        const char* path = std::getenv("ALCHEMY_HIGH_SHADOW_BASELINE_LIBRARY");
        REQUIRE(path);
        REQUIRE(std::filesystem::is_regular_file(path));
        SharedLibraryFixture f;
        const auto shared = f.data->builtinLibrary(&f.error);
        const auto epoch = f.data->builtinLibraryEpoch();
        for (const char* name : {"almetal_world_light_map_fragment",
                 "almetal_world_projector_light_fragment",
                 "almetal_world_pre_water_simple_shadow_fragment",
                 "almetal_world_pre_water_material_fragment",
                 "almetal_world_pre_water_pbr_fragment"})
        {
            INFO(name);
            REQUIRE(shared->hasBoolFunctionConstant(name, 3));
        }
        auto scene = f.rowScene();
        auto current = f.renderer->render(scene, &f.error);
        INFO(f.error); REQUIRE(current);
        const auto expected = f.read(*current);
        REQUIRE(f.renderer->reloadShaders(std::string(path), &f.error));
        INFO(f.error);
        CHECK_FALSE(f.renderer->shaderLibrary()->hasBoolFunctionConstant(
            "almetal_world_light_map_fragment", 3));
        CHECK(f.renderer->shaderLibrary() != shared);
        CHECK(f.probe->shaderLibrary() == shared);
        CHECK(f.data->builtinLibrary(&f.error) == shared);
        CHECK(f.data->builtinLibraryEpoch() == epoch);
        auto previous = f.renderer->render(scene, &f.error);
        INFO(f.error); REQUIRE(previous);
        CHECK(f.read(*previous) == expected);
        auto probe = f.probe->render(scene, &f.error);
        INFO(f.error); REQUIRE(probe);
        CHECK(f.read(*probe) == expected);
    }
}
