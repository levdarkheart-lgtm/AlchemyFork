/**
 * @brief PPD-097: visibility tickets settle even if their producer is abandoned.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalTexture> target;
    ALMetalWorldVisibilityQuery query;
    ALMetalWorldSpotShadowScene scene;
    Fixture()
    {
        device = ALMetalDevice::create(&error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error); REQUIRE(renderer);
        ALMetalTextureDescriptor d;
        d.type = ALMetalTextureType::texture_2d_array;
        d.format = ALMetalPixelFormat::r32_float;
        d.width = d.height = 8; d.array_length = 2;
        d.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        target = ALMetalTexture::create(*device, d, &error); REQUIRE(target);
        for (unsigned axis = 0; axis < 4; ++axis)
            query.modelview_projection[axis * 4 + axis] = 1.f;
        query.center[2] = .5f;
        query.half_extent[0] = query.half_extent[1] = .25f;
        query.half_extent[2] = .03125f; query.fan = 7;
        scene.width = scene.height = 8;
        scene.visibility_queries = &query; scene.visibility_query_count = 1;
    }
};
}

TEST_CASE("almetal: abandoned visibility producers fail instead of staying pending",
          "[almetal][visibility-abandon]")
{
    @autoreleasepool
    {
        Fixture f;
        std::shared_ptr<ALMetalVisibilityResultBatch> first, second;
        REQUIRE(f.renderer->beginCommandBatch("Abandoned visibility producer", &f.error));
        REQUIRE(f.renderer->renderSpotShadow(*f.target, 0, f.scene, &f.error, &first));
        REQUIRE(f.renderer->renderSpotShadow(*f.target, 1, f.scene, &f.error, &second));
        REQUIRE(first); REQUIRE(second);
        CHECK(first->status() == ALMetalVisibilityResultBatch::Status::pending);
        CHECK(second->status() == ALMetalVisibilityResultBatch::Status::pending);
        f.renderer->cancelCommandBatch();
        CHECK(first->status() == ALMetalVisibilityResultBatch::Status::failed);
        CHECK(second->status() == ALMetalVisibilityResultBatch::Status::failed);
        std::uint64_t value = 99;
        CHECK_FALSE(first->result(0, value));
        CHECK_FALSE(first->wait(0, value, &f.error));
    }
}

TEST_CASE("almetal: submitted visibility results survive their C++ wrapper",
          "[almetal][visibility-abandon]")
{
    @autoreleasepool
    {
        Fixture f;
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
        REQUIRE(f.renderer->beginCommandBatch("Completed visibility producer", &f.error));
        REQUIRE(f.renderer->renderSpotShadow(*f.target, 0, f.scene, &f.error, &results));
        REQUIRE(results);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        f.renderer.reset();
        std::uint64_t value = 99;
        REQUIRE(results->wait(0, value, &f.error));
        CHECK(results->status() == ALMetalVisibilityResultBatch::Status::ready);
        REQUIRE(results->result(0, value));
        CHECK(value == 1);
    }
}
