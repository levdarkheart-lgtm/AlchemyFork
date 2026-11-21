/**
 * @file almetal_world_library_test.mm
 * @brief Shared world library epochs and atomic main/probe executable reload.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalshadowmaterials.h"
#include "../almetalworldmaterialdata.h"
#include <cstdlib>
#include <filesystem>

namespace
{
// Original1eb44 forced program rebuilding leaves targets/history owned by the
// pipeline. The native library/cache has no GL equivalent: one exact
// main/probe lease must be staged and published as a coherent owner group.
struct SharedLibraryFixture : Fixture
{
    std::unique_ptr<ALMetalWorldGBufferRenderer> probe;
    ALMetalWorldMaterialDataCache material_cache;
    ALMetalWorldSimpleBatch source;

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

    }
    auto owners() { return std::array{renderer.get(), probe.get()}; }
    ALMetalWorldGBufferScene worldScene()
    {
        auto value = scene(); value.normal_hdr = false;
        value.batches = &source; value.batch_count = 1;
        value.source_owner = resources;
        return value;
    }
};
}

TEST_CASE("almetal: main and probe consume one exact native shader library",
          "[almetal][world-library]")
{
    @autoreleasepool
    {
        SharedLibraryFixture f;
        const auto lease = f.data->builtinLibrary(&f.error);
        REQUIRE(lease); CHECK(f.data->builtinLibraryEpoch() == 1);
        CHECK(f.renderer->shaderLibrary() == lease); CHECK(f.probe->shaderLibrary() == lease);
        CHECK(f.renderer->shaderLibrary()->nativeHandle() == f.probe->shaderLibrary()->nativeHandle());

        auto scene = f.worldScene();
        auto scalar = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(scalar);
        const auto expected = f.read(*scalar);
        auto main = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(main);

        auto probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(probe);

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

        auto* shadows = f.data->shadowMaterials(*old, &f.error); REQUIRE(shadows);
        auto scene = f.worldScene();
        auto main = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(main);
        auto probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(probe);

        const auto expected = f.read(*main);

        CHECK_FALSE(f.renderer->reloadShaders(&f.error));
        CHECK_FALSE(f.probe->reloadShaders(&f.error));
        CHECK(f.data->builtinLibraryEpoch() == epoch);
        CHECK(f.renderer->shaderLibrary() == old); CHECK(f.probe->shaderLibrary() == old);
        CHECK(f.data->shadowMaterials(*old, &f.error) == shadows);

        f.probe->failNextShaderReloadForTesting();
        CHECK_FALSE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error));
        CHECK_FALSE(f.error.empty());
        CHECK(f.data->builtinLibraryEpoch() == epoch); CHECK(f.data->builtinLibrary(&f.error) == old);
        CHECK(f.renderer->shaderLibrary() == old); CHECK(f.probe->shaderLibrary() == old);
        CHECK(f.renderer->shaderGeneration() == 1); CHECK(f.probe->shaderGeneration() == 1);
        CHECK(f.data->shadowMaterials(*old, &f.error) == shadows);


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

        auto continued = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(continued);
        CHECK(f.read(*continued) == expected);

        auto continued_probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(continued_probe);
        CHECK(f.read(*continued_probe) == expected);

    }
}

TEST_CASE("almetal: old scalar commands survive shared shader publication and abandonment",
          "[almetal][world-library][shader-reload]")
{
    @autoreleasepool
    {
        SharedLibraryFixture f;
        auto library = f.data->builtinLibrary(&f.error);
        auto scene = f.worldScene();
        auto reference = f.renderer->render(scene, &f.error); REQUIRE(reference);
        const auto expected = f.read(*reference);
        auto pending = ALMetalWorldGBufferRenderer::create(*f.device, 4, library, &f.error); REQUIRE(pending);
        auto abandoned = ALMetalWorldGBufferRenderer::create(*f.device, 4, library, &f.error); REQUIRE(abandoned);
        REQUIRE(pending->beginCommandBatch("Pending prior scalar library", &f.error));
        auto target = pending->render(scene, &f.error); INFO(f.error); REQUIRE(target);
        auto pending_receipt = pending->commandBatchCompletion(); REQUIRE(pending_receipt);
        REQUIRE(abandoned->beginCommandBatch("Abandoned prior scalar library", &f.error));
        auto canceled = abandoned->render(scene, &f.error); REQUIRE(canceled);
        auto canceled_receipt = abandoned->commandBatchCompletion(); REQUIRE(canceled_receipt);
        CHECK_FALSE(pending_receipt->isComplete()); CHECK_FALSE(canceled_receipt->isComplete());
        auto owners = f.owners();
        REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error)); INFO(f.error);
        CHECK(f.renderer->shaderLibrary() != library); CHECK(f.probe->shaderLibrary() != library);
        CHECK(pending->shaderLibrary() == library); CHECK(abandoned->shaderLibrary() == library);
        abandoned->cancelCommandBatch(); CHECK(canceled_receipt->isComplete());
        CHECK_FALSE(pending_receipt->isComplete());
        REQUIRE(pending->commitCommandBatch(&f.error)); REQUIRE(f.device->waitUntilIdle(&f.error));
        CHECK(pending_receipt->isComplete()); CHECK(f.read(*target) == expected);
        auto current = f.renderer->render(scene, &f.error); REQUIRE(current);
        CHECK(f.read(*current) == expected);
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

        auto* shadows = f.data->shadowMaterials(*shared, &f.error); REQUIRE(shadows);
        auto scene = f.worldScene();
        auto baseline = f.renderer->render(scene, &f.error); REQUIRE(baseline);
        const auto expected = f.read(*baseline);
        REQUIRE(f.renderer->reloadShaders(std::string(path), &f.error)); INFO(f.error);
        CHECK(f.renderer->shaderLibrary() != shared); CHECK(f.probe->shaderLibrary() == shared);
        CHECK(f.data->builtinLibrary(&f.error) == shared); CHECK(f.data->builtinLibraryEpoch() == epoch);
        CHECK(f.data->shadowMaterials(*shared, &f.error) == shadows);

        auto private_old = f.renderer->render(scene, &f.error); INFO(f.error); REQUIRE(private_old);
        CHECK(f.read(*private_old) == expected);

        CHECK(f.read(*private_old) == expected);
        auto current_probe = f.probe->render(scene, &f.error); INFO(f.error); REQUIRE(current_probe);
        CHECK(f.read(*current_probe) == expected);

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
        auto scene = f.worldScene();
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
