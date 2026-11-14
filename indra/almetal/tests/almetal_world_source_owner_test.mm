/**
 * @file almetal_world_source_owner_test.mm
 * @brief Producer-generation ownership across direct and forward commands.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalpipeline.h"

namespace
{
// A real earlier command on the same queue keeps subsequent renderer commands
// pending. No renderer hook or simulated completion decides lease retirement.
struct QueueGate
{
    id<MTLSharedEvent> event;
    std::unique_ptr<ALMetalCommandBuffer> blocker;

    explicit QueueGate(Fixture& fixture)
    {
        auto native_device = (__bridge id<MTLDevice>)fixture.device->nativeHandle();
        event = [native_device newSharedEvent];
        REQUIRE(event);
        blocker = fixture.device->makeCommandBuffer("World source lease queue gate", &fixture.error);
        INFO(fixture.error); REQUIRE(blocker);
        auto native = (__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
        [native encodeWaitForEvent:event value:1];
        REQUIRE(blocker->commit(&fixture.error));
    }
    ~QueueGate() { event.signaledValue = 1; }
    void release(Fixture& fixture)
    {
        event.signaledValue = 1;
        REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
    }
};

void requireRetired(const std::weak_ptr<Resources>& source,
                    const std::weak_ptr<int>& credit)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((!source.expired() || !credit.expired()) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    CHECK(source.expired());
    CHECK(credit.expired());
}

std::vector<std::uint8_t> colorBytes(Fixture& fixture, const ALMetalRenderTarget& target)
{
    const auto* texture = target.colorTexture(0);
    REQUIRE(texture);
    const std::size_t bytes = texture->descriptor().format == ALMetalPixelFormat::rgba16_float ? 8 : 4;
    std::vector<std::uint8_t> result(side * side * bytes);
    REQUIRE(texture->readback(*fixture.device, 0, 0, {}, result.data(), side * bytes, &fixture.error));
    return result;
}

std::unique_ptr<ALMetalRenderTarget> lighting(Fixture& fixture)
{
    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = descriptor.height = side;
    descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
    auto target = ALMetalRenderTarget::create(*fixture.device, descriptor, &fixture.error);
    INFO(fixture.error); REQUIRE(target);
    ALMetalRenderPassConfig config;
    config.colors[0].clear = {.03125, .0625, .125, .5};
    config.depth_store = config.stencil_store = ALMetalStoreAction::store;
    auto pass = target->makeRenderPass(config, &fixture.error);
    REQUIRE(pass);
    auto command = fixture.device->makeCommandBuffer("World source lease lighting clear", &fixture.error);
    REQUIRE(command);
    auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "World source lighting", &fixture.error);
    INFO(fixture.error); REQUIRE(encoder);
    REQUIRE(encoder->end(&fixture.error));
    REQUIRE(command->commitAndWait(&fixture.error));
    return target;
}

ALMetalWorldOpaquePostDeferredScene forwardScene(
    const ALMetalRenderTarget& gbuffer, const ALMetalRenderTarget& composed,
    const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldOpaquePostDeferredScene scene;
    scene.gbuffer = &gbuffer;
    scene.composed_lighting = &composed;
    scene.batches = &batch;
    scene.batch_count = 1;
    scene.width = scene.height = side;
    return scene;
}

ALMetalWorldSimpleBatch fullbright(Fixture& fixture)
{
    auto batch = fixture.batch(0);
    batch.impostor_fullbright = true;
    return batch;
}
}

TEST_CASE("almetal: direct and forward abandonment retire source and scope owners independently",
          "[almetal][world-source-owner]")
{
    @autoreleasepool
    {
        for (bool forward : {false, true})
        {
            CAPTURE(forward);
            Fixture fixture;
            auto empty = fixture.scene();
            auto gbuffer = fixture.renderer->render(empty, &fixture.error);
            INFO(fixture.error); REQUIRE(gbuffer);
            auto composed = lighting(fixture);
            auto batch = forward ? fullbright(fixture) : fixture.batch(0);
            std::weak_ptr<Resources> source(fixture.resources);
            auto credit = std::make_shared<int>(23);
            std::weak_ptr<int> weak_credit(credit);
            std::unique_ptr<ALMetalRenderTarget> output;
            {
                ALMetalSubmissionScope scope(credit);
                REQUIRE(fixture.renderer->beginCommandBatch("Abandoned world source generation", &fixture.error));
                if (forward)
                {
                    auto scene = forwardScene(*gbuffer, *composed, batch);
                    scene.source_owner = fixture.resources;
                    output = fixture.renderer->renderOpaquePostDeferred(scene, &fixture.error);
                }
                else
                {
                    auto scene = fixture.scene();
                    scene.batches = &batch;
                    scene.batch_count = 1;
                    scene.source_owner = fixture.resources;
                    output = fixture.renderer->render(scene, &fixture.error);
                }
                INFO(fixture.error); REQUIRE(output);
            }
            fixture.resources.reset();
            credit.reset();
            CHECK_FALSE(source.expired());
            CHECK_FALSE(weak_credit.expired());
            auto completion = fixture.renderer->commandBatchCompletion();
            REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());
            fixture.renderer->cancelCommandBatch();
            CHECK(source.expired());
            CHECK(weak_credit.expired());
        }
    }
}

TEST_CASE("almetal: both forward overloads pin producer bytes through delayed native completion",
          "[almetal][world-source-owner]")
{
    @autoreleasepool
    {
        for (bool continuation : {false, true})
        {
            CAPTURE(continuation);
            Fixture fixture;
            auto gbuffer = fixture.renderer->render(fixture.scene(), &fixture.error);
            INFO(fixture.error); REQUIRE(gbuffer);
            auto reference_light = lighting(fixture);
            auto batch = fullbright(fixture);
            auto reference_scene = forwardScene(*gbuffer, *reference_light, batch);
            auto reference = fixture.renderer->renderOpaquePostDeferred(reference_scene, &fixture.error);
            INFO(fixture.error); REQUIRE(reference);
            const auto expected = colorBytes(fixture, *reference);
            // Require a real forward draw, not merely a successful clear/copy.
            const auto center = (side / 2 * side + side / 2) * 8;
            CHECK_FALSE(std::equal(expected.begin(), expected.begin() + 8, expected.begin() + center));
            auto candidate_light = lighting(fixture);
            auto scene = forwardScene(*gbuffer, *candidate_light, batch);
            scene.source_owner = fixture.resources;
            std::weak_ptr<Resources> source(fixture.resources);
            auto credit = std::make_shared<int>(31);
            std::weak_ptr<int> weak_credit(credit);
            QueueGate gate(fixture);
            std::unique_ptr<ALMetalRenderTarget> candidate;
            {
                ALMetalSubmissionScope scope(credit);
                candidate = continuation
                    ? fixture.renderer->renderOpaquePostDeferred(scene, std::move(candidate_light), &fixture.error)
                    : fixture.renderer->renderOpaquePostDeferred(scene, &fixture.error);
                INFO(fixture.error); REQUIRE(candidate);
            }
            scene.source_owner.reset();
            fixture.resources.reset();
            credit.reset();
            fixture.renderer.reset();
            CHECK_FALSE(source.expired());
            CHECK_FALSE(weak_credit.expired());
            gate.release(fixture);
            requireRetired(source, weak_credit);
            CHECK(colorBytes(fixture, *candidate) == expected);
        }
    }
}

TEST_CASE("almetal: impostor forward commands pin producer bytes after caller and renderer destruction",
          "[almetal][world-source-owner]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto reference = fixture.renderer->render(fixture.scene(), &fixture.error);
        auto candidate = fixture.renderer->render(fixture.scene(), &fixture.error);
        INFO(fixture.error); REQUIRE(reference); REQUIRE(candidate);
        auto batch = fullbright(fixture);
        auto scene = fixture.scene();
        scene.batches = &batch;
        scene.batch_count = 1;
        const auto clear = colorBytes(fixture, *reference);
        REQUIRE(fixture.renderer->compositeImpostorPostDeferred(*reference, scene, &fixture.error));
        const auto expected = colorBytes(fixture, *reference);
        CHECK(expected != clear);
        scene.source_owner = fixture.resources;
        std::weak_ptr<Resources> source(fixture.resources);
        auto credit = std::make_shared<int>(47);
        std::weak_ptr<int> weak_credit(credit);
        QueueGate gate(fixture);
        {
            ALMetalSubmissionScope scope(credit);
            REQUIRE(fixture.renderer->compositeImpostorPostDeferred(*candidate, scene, &fixture.error));
        }
        scene.source_owner.reset();
        fixture.resources.reset();
        credit.reset();
        fixture.renderer.reset();
        CHECK_FALSE(source.expired());
        CHECK_FALSE(weak_credit.expired());
        gate.release(fixture);
        requireRetired(source, weak_credit);
        CHECK(colorBytes(fixture, *candidate) == expected);
    }
}

TEST_CASE("almetal: failed G-buffer transactions reject a careless outer commit and recover",
          "[almetal][world-source-owner][world-transaction]")
{
    @autoreleasepool
    {
        // The real input/epoch checks fail deterministically. In each case an
        // earlier valid pass has already encoded into the enclosing command;
        // rejecting only the failed pass must not publish that valid prefix.
        for (unsigned failure : {0u, 1u, 2u})
        {
            CAPTURE(failure);
            Fixture fixture;
            auto batch = fixture.batch(0);
            auto scene = fixture.scene();
            scene.batches = &batch;
            scene.batch_count = 1;
            auto reference = fixture.renderer->render(scene, &fixture.error);
            INFO(fixture.error); REQUIRE(reference);
            const auto expected = colorBytes(fixture, *reference);
            auto owner = std::make_shared<int>(59);
            std::weak_ptr<int> weak_owner(owner);
            scene.source_owner = owner;
            REQUIRE(fixture.renderer->beginCommandBatch("Failed world pass must discard encoded prefix", &fixture.error));
            auto prefix = fixture.renderer->render(scene, &fixture.error);
            INFO(fixture.error); REQUIRE(prefix);
            scene.source_owner.reset();
            owner.reset();
            CHECK_FALSE(weak_owner.expired());
            auto completion = fixture.renderer->commandBatchCompletion();
            REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());

            if (failure < 2)
            {
                auto invalid = batch;
                invalid.diffuse_textures[0] = nullptr;
                auto invalid_scene = scene;
                invalid_scene.batches = &invalid;
                CHECK_FALSE(fixture.renderer->render(invalid_scene, failure ? nullptr : &fixture.error));
                if (!failure) CHECK_FALSE(fixture.error.empty());
            }
            else
            {
                auto epoch = std::make_shared<std::atomic<std::uint64_t>>(17);
                auto inputs = ALMetalWorldViewInputs::adopt(scene, {batch}, {}, fixture.resources, 17, &fixture.error);
                INFO(fixture.error); REQUIRE(inputs);
                auto ticket = ALMetalWorldPreparation::create(inputs, 1, epoch, &fixture.error);
                REQUIRE(ticket);
                REQUIRE(ticket->runJob(0));
                auto prepared = ticket->finish(&fixture.error);
                REQUIRE(prepared);
                epoch->store(18, std::memory_order_release);
                CHECK_FALSE(fixture.renderer->renderPrepared(*prepared, &fixture.error));
                CHECK(fixture.error == "Prepared world view epoch was cancelled");
            }

            const auto submitted_before = ALMetalDevice::committedCommandBufferCount();
            CHECK_FALSE(fixture.renderer->commitCommandBatch(&fixture.error));
            CHECK(ALMetalDevice::committedCommandBufferCount() == submitted_before);
            CHECK_FALSE(fixture.renderer->commandBatchActive());
            CHECK(completion->isComplete()); // Unsubmitted abandonment only.
            CHECK(weak_owner.expired());

            fixture.error.clear();
            REQUIRE(fixture.renderer->beginCommandBatch("Fresh world transaction after failure", &fixture.error));
            scene.source_owner = fixture.resources;
            auto recovered = fixture.renderer->render(scene, &fixture.error);
            INFO(fixture.error); REQUIRE(recovered);
            REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error));
            REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
            CHECK(colorBytes(fixture, *recovered) == expected);
        }
    }
}
